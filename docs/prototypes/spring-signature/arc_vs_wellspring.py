"""Smooth-arc D vs the Wellspring: the 'pew' on clicks, pitch steadiness on skank, roundness, ringiness."""
import numpy as np, soundfile as sf, glob
from scipy.signal import butter, sosfiltfilt, stft
R="/Users/jesse/Documents/Sites/resilio-versio"; P=f"{R}/renders/proto_smooth_arc/clean"
S="/private/tmp/claude-501/-Users-jesse-Documents-Sites-resilio-versio/91ef3bc9-5b58-43de-aeb9-3361a42117a2/scratchpad/arc2"
def f(stim,v): return glob.glob(f"{P}/{stim}_{v}_*.wav")[0]
SRC={"A bright tail":lambda s:f(s,"A"),"D light smear":lambda s:f(s,"D"),"D, WOBBLE+SPLASH off":lambda s:f"{S}/D_still_{s}.wav","W wellspring":lambda s:f(s,"W")}
def load(p): x,sr=sf.read(p,always_2d=True); return x.astype(float),sr
def onsets(x,sr,db,gap):
    a=np.abs(x).max(1); th=a.max()*10**(db/20); o=[];l=-1e9
    for i in np.flatnonzero(a>th):
        if i-l>gap*sr:o.append(i)
        l=i
    return o
cx,sr=load(f"{R}/test_audio/stimulus/01_clicks.wav"); CL=onsets(cx,sr,-40,1.0)
sx,_=load(f"{R}/test_audio/stimulus/04_skank.wav"); SK=onsets(sx,sr,-25,.2)
def bp(y,lo,hi,o=4): return sosfiltfilt(butter(o,[lo,hi],btype='band',fs=sr,output='sos'),y,axis=0)

print("1. THE PEW: first echo, when each band peaks (ms after the 500 Hz band), median over clicks")
bands=[250,1000,2000,3000,4000,5000]
for k,g in SRC.items():
    y,_=load(g("01_clicks")); m=y.mean(1); rows=[]
    for o in CL:
        seg=m[o:o+int(.12*sr)]
        env0=np.abs(bp(seg,500/1.2,500*1.2,3)); t0=np.argmax(env0[:int(.08*sr)])
        rows.append([ (np.argmax(np.abs(bp(seg,c/1.2,c*1.2,3))[max(0,t0-int(.01*sr)):t0+int(.04*sr)])+max(0,t0-int(.01*sr))-t0)/sr*1000 for c in bands])
    print(f"  {k:22}"+"  ".join(f"{b}:{v:5.1f}" for b,v in zip(bands,np.median(rows,axis=0))))

print("\n2. PITCH STEADINESS on the skank tail: strongest partials 200 Hz-1.5 kHz tracked 0.3-0.7 s after each chord;")
print("   frequency wobble (cents, rms around each partial's own trend), median over chords and partials")
for k,g in SRC.items():
    y,_=load(g("04_skank")); m=y.mean(1); vals=[]
    for i,o in enumerate(SK[:-1]):
        seg=m[o+int(.3*sr):o+int(.7*sr)]
        if len(seg)<int(.4*sr)-5: continue
        fr,t,Z=stft(seg,sr,nperseg=4096,noverlap=4096-256); P2=np.abs(Z)**2
        band=(fr>200)&(fr<1500); spec=P2[band].mean(1); idx=np.argsort(spec)[-4:]
        for j in idx:
            jj=np.flatnonzero(band)[j]; fr_track=[]
            for c in range(P2.shape[1]):
                a,b,cc=P2[jj-1,c],P2[jj,c],P2[jj+1,c]
                d=0.5*(a-cc)/(a-2*b+cc+1e-30); fr_track.append(fr[jj]+d*(fr[1]-fr[0]))
            fr_track=np.array(fr_track); cents=1200*np.log2(fr_track/np.mean(fr_track))
            tt=np.arange(len(cents)); cents=cents-np.polyval(np.polyfit(tt,cents,1),tt)
            vals.append(np.sqrt(np.mean(cents**2)))
    print(f"  {k:22} {np.median(vals):5.2f} cents")

print("\n3. ROUNDNESS: echo attack rise time (ms, 10->90 % of each echo's envelope peak, 1-4 kHz, clicks 30-300 ms), and late-tail tone (dB re 500 Hz-1 kHz)")
for k,g in SRC.items():
    y,_=load(g("01_clicks")); m=y.mean(1); b=np.abs(bp(m,1000,4000)); n=int(.001*sr)
    env=np.convolve(b,np.ones(n)/n,'same'); rises=[]
    for o in CL:
        e=env[o+int(.03*sr):o+int(.3*sr)]
        from scipy.signal import find_peaks
        pk,_=find_peaks(e,height=e.max()*.2,distance=int(.008*sr))
        for p in pk:
            seg=e[max(0,p-int(.01*sr)):p+1]; top=e[p]
            i10=np.argmax(seg>0.1*top); i90=np.argmax(seg>0.9*top); rises.append((i90-i10)/sr*1000)
    # tail tone
    ts=[]
    for c in [250,500,1000,2000,3000,4000,6000]:
        ts.append(10*np.log10(np.mean([np.mean(bp(m[o+int(.5*sr):o+int(1.5*sr)],c/1.2,c*1.2,3)**2) for o in CL])+1e-30))
    ts=np.array(ts); ts=ts-ts[1:3].mean()
    print(f"  {k:22} rise {np.median(rises):4.1f} ms | tail tone 250/500/1k/2k/3k/4k/6k: "+" ".join(f"{v:5.1f}" for v in ts))

print("\n4. RINGINESS: late-tail fine-spectrum peakiness (std dB, 1/48 vs 1/3 octave, 200 Hz-4 kHz), clicks and skank")
def peaky(seg):
    X=np.abs(np.fft.rfft(seg*np.hanning(len(seg))))**2; fq=np.fft.rfftfreq(len(seg),1/sr); cs=np.concatenate([[0],np.cumsum(X)])
    def sm(fr_):
        out=np.empty_like(X)
        for i,z in enumerate(fq):
            if z<=0: out[i]=X[i]; continue
            a=np.searchsorted(fq,z*2**(-fr_/2)); b=max(np.searchsorted(fq,z*2**(fr_/2)),a+1); out[i]=(cs[b]-cs[a])/(b-a)
        return out
    msk=(fq>200)&(fq<4000); d=10*np.log10(sm(1/48)+1e-30)-10*np.log10(sm(1/3)+1e-30); return d[msk].std()
for k,g in SRC.items():
    y,_=load(g("01_clicks")); m=y.mean(1); pc=np.median([peaky(m[o+int(.3*sr):o+int(1.5*sr)]) for o in CL[:4]])
    y,_=load(g("04_skank")); m=y.mean(1); ps=np.median([peaky(m[o+int(.3*sr):o+int(.75*sr)]) for o in SK[:6]])
    print(f"  {k:22} clicks {pc:4.1f} dB   skank {ps:4.1f} dB")
