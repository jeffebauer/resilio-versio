#include "Sidecar.h"
#include "Base64.h"

#include <cmath>

namespace rv::sidecar {

json::Value metricsToJson(const metrics::Metrics& m)
{
    json::Value v = json::Value::makeObject();
    v.set("peak_dbfs", json::Value::makeNumber(m.peakDbfs));
    v.set("rms_dbfs", json::Value::makeNumber(m.rmsDbfs));
    v.set("t60_s", std::isnan(m.t60S) ? json::Value::makeNull() : json::Value::makeNumber(m.t60S));
    v.set("resonance_peak_db", std::isnan(m.resonancePeakDb) ? json::Value::makeNull() : json::Value::makeNumber(m.resonancePeakDb));
    v.set("steady_tone", json::Value::makeBool(m.steadyTone));
    v.set("nan_inf_count", json::Value::makeNumber(double(m.nanInfCount)));
    v.set("clip_count", json::Value::makeNumber(double(m.clipCount)));
    v.set("click_count", json::Value::makeNumber(double(m.clickCount)));
    return v;
}

metrics::Metrics jsonToMetrics(const json::Value& v)
{
    metrics::Metrics m;
    m.peakDbfs = v.get("peak_dbfs", -200.0);
    m.rmsDbfs = v.get("rms_dbfs", -200.0);
    const json::Value* t60 = v.find("t60_s");
    m.t60S = (t60 && !t60->isNull()) ? t60->numberValue() : std::nan("");
    const json::Value* res = v.find("resonance_peak_db");
    m.resonancePeakDb = (res && !res->isNull()) ? res->numberValue() : std::nan("");
    m.steadyTone = v.get("steady_tone", false);
    m.nanInfCount = long(v.get("nan_inf_count", 0.0));
    m.clipCount = long(v.get("clip_count", 0.0));
    m.clickCount = long(v.get("click_count", 0.0));
    return m;
}

json::Value spectrogramToJson(const spectrogram::Spectrogram& s)
{
    json::Value v = json::Value::makeObject();
    v.set("width", json::Value::makeNumber(s.width));
    v.set("height", json::Value::makeNumber(s.height));
    v.set("t0_s", json::Value::makeNumber(s.t0S));
    v.set("t1_s", json::Value::makeNumber(s.t1S));
    v.set("f_min_hz", json::Value::makeNumber(s.fMinHz));
    v.set("f_max_hz", json::Value::makeNumber(s.fMaxHz));
    v.set("freq_scale", json::Value::makeString("log"));
    v.set("db_min", json::Value::makeNumber(s.dbMin));
    v.set("db_max", json::Value::makeNumber(s.dbMax));
    v.set("data_b64", json::Value::makeString(base64::encode(s.data)));
    return v;
}

spectrogram::Spectrogram jsonToSpectrogram(const json::Value& v)
{
    spectrogram::Spectrogram s;
    s.width = int(v.get("width", double(s.width)));
    s.height = int(v.get("height", double(s.height)));
    s.t0S = v.get("t0_s", 0.0);
    s.t1S = v.get("t1_s", 0.0);
    s.fMinHz = v.get("f_min_hz", s.fMinHz);
    s.fMaxHz = v.get("f_max_hz", s.fMaxHz);
    s.dbMin = v.get("db_min", s.dbMin);
    s.dbMax = v.get("db_max", s.dbMax);
    base64::decode(v.get("data_b64", std::string()), s.data);
    return s;
}

json::Value paramsToJson(const Tank& tank)
{
    json::Value v = json::Value::makeObject();
    for (const auto& p : kParams) {
        if (p.kind == ParamKind::Switch3) {
            const int pos = normalisedToSwitch(tank.param(p.id));
            v.set(p.key, json::Value::makeString(p.choices[pos]));
        } else {
            v.set(p.key, json::Value::makeNumber(double(tank.param(p.id))));
        }
    }
    return v;
}

json::Value build(const std::string& wavFilename, int sampleRate, double durationS,
                   const json::Value& params, const metrics::Metrics& m,
                   const spectrogram::Spectrogram& spec)
{
    json::Value v = json::Value::makeObject();
    v.set("wav", json::Value::makeString(wavFilename));
    v.set("sample_rate", json::Value::makeNumber(sampleRate));
    v.set("duration_s", json::Value::makeNumber(durationS));
    v.set("params", params);
    v.set("metrics", metricsToJson(m));
    v.set("spectrogram", spectrogramToJson(spec));
    return v;
}

} // namespace rv::sidecar
