"""Owner round 2 on the sweet-tank page: tail wavers (decay smoothness), perceived width, the Wellspring's high pitch bend."""
import numpy as np, soundfile as sf
from scipy.signal import butter, sosfiltfilt, stft
R="/Users/jesse/Documents/Sites/resilio-versio"; P=f"{R}/renders/proto_sweet_tank/clean"
V={"A":"A_bright_tail","B":"B_no_single_beat","C":"C_no_growing_sweep","D":"D_tank_stereo","E":"E_light_smear","W":"W_wellspring"}
import glob,os
names={k:(glob.glob(f"{P}/01_clicks_{k}_*.wav")+[None])[0] for k in V}
def load(p): x,sr=sf.read(p,always_2d=True); return x.astype(float),sr
x,sr=load(f"{R}/test_audio/stimulus/01_clicks.wav")
a=np.abs(x).max(1); th=a.max()*.01; ons=[];l=-1e9
for i in np.flatnonzero(a>th):
    if i-l>sr: ons.append(i)
    l=i
def bp(y,lo,hi): return sosfiltfilt(butter(4,[lo,hi],btype='band',fs=sr,output='sos'),y,axis=0)
print("1. WAVERS: tail envelope vs a smooth exponential decay (0.15-1.5 s after each click, 20 ms envelope).")
print("   residual = rms dB wobble around the fitted straight-line decay; lower = smoother. Per band, median over clicks")
print(f"{'':4}"+"".join(f"{b:>12}" for b in ["250-500","500-1k","1k-2k","2k-4k"]))
for k,p in names.items():
    if not p: continue
    y,_=load(p); m=y.mean(1); row=[]
    for lo,hi in [(250,500),(500,1000),(1000,2000),(2000,4000)]:
        b=bp(m,lo,hi); res=[]
        for o in ons:
            seg=b[o+int(.15*sr):o+int(1.5*sr)]**2
            n=int(.02*sr); e=np.array([seg[i:i+n].mean() for i in range(0,len(seg)-n,n)])
            d=10*np.log10(e+1e-20); t=np.arange(len(d))*0.02; ok=d>d.max()-45
            fit=np.polyval(np.polyfit(t[ok],d[ok],1),t[ok]); res.append(np.sqrt(np.mean((d[ok]-fit)**2)))
        row.append(np.median(res))
    print(f"{k:4}"+"".join(f"{v:12.2f}" for v in row))
print("\n2. WIDTH: side vs mid energy (dB, 0 = as much difference as common; higher = wider) and L/R corr, by band, 0.2-1.5 s (late tail) and 0.03-0.2 s")
for win in [(.03,.2),(.2,1.5)]:
    print(f"  window {win}")
    for k,p in names.items():
        if not p: continue
        y,_=load(p); row=[]
        for lo,hi in [(125,250),(250,500),(500,1000),(1000,2000),(2000,4000)]:
            b=bp(y,lo,hi); L=np.concatenate([b[o+int(win[0]*sr):o+int(win[1]*sr),0] for o in ons]); Rr=np.concatenate([b[o+int(win[0]*sr):o+int(win[1]*sr),1] for o in ons])
            row.append(f"{10*np.log10(np.sum((L-Rr)**2)/np.sum((L+Rr)**2)):5.1f}/{np.corrcoef(L,Rr)[0,1]:5.2f}")
        print(f"   {k:3}"+"  ".join(row))
print("\n3. HIGH PITCH BEND in the first 120 ms after each click: in 1.5-8 kHz, the dominant frequency every 2 ms (median over clicks),")
print("   and the overall slope (octaves per 10 ms) of that ridge from the first arrival onwards")
for k in ["A","C","W"]:
    p=names[k]; y,_=load(p); m=y.mean(1)
    f,t,Z=stft(m,sr,nperseg=512,noverlap=512-96)  # 2 ms hop, 10.7 ms window
    band=(f>=1500)&(f<=8000); tracks=[]
    for o in ons:
        i0=int(o/96); seg=np.abs(Z[band][:,i0:i0+60])**2
        e=seg.sum(0); ridge=f[band][np.argmax(seg,axis=0)]
        tracks.append(np.where(e>e.max()*10**(-3),ridge,np.nan))
    tr=np.nanmedian(np.array(tracks),axis=0)
    ok=~np.isnan(tr); tt=np.arange(len(tr))*2
    print(f"   {k}: ridge Hz at 0/10/20/30/40/60/80/100 ms:",[int(tr[i]) if ok[i] else None for i in [0,5,10,15,20,30,40,50]])
