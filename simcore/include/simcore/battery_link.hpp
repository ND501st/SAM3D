// simcore/battery_link.hpp — the player's Osa and the battery's PU-12M in one battery (docs/PU12_DEVELOPMENT.md P8).
//
// Each vehicle keeps its own runner (ScenarioRunner): its console, its crew, its seats. The link steps the two together
// and carries between them what passes in the sky and on the battery net:
//   - the targets: the Osa's runner moves them (its crew's locks and launches make them evade); the post's follows it
//     each step (MirrorTargets), and a target one side destroys is destroyed on the other;
//   - the post's designations to the Osa (by voice: number, azimuth and range) reach the Osa's commander as the PU-12's,
//     with the message's own timing and over the Osa's link to the post; none while the post's battery net is off the air;
//   - the Osa's commander's reports (к бою готов, пуск, цель поражена, ракет нет) go onto the post's IT boards, and the
//     post's other launchers' reports are heard in the Osa;
//   - the Osa's place in the post's fire distribution: its missiles and the target its crew is on.
//
// The file (data/scenarios/<link>.json):
//   { "link_id", "name", "description", "osa": "scenarios/<the Osa's>.json", "post": "scenarios/<the post's>.json",
//     "callsign": "21", "source", "confidence", "notes" }
// The Osa's scenario has "battery_commander": "post" and the callsign as its own in the battery; the post's has a
// launcher of that callsign in its battery and no targets of its own (it takes the Osa's); both share an origin.
#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {

struct BatteryLinkDef {
    std::string id;
    std::string name;
    std::string description;
    std::string osa_file;   // relative to the data folder
    std::string post_file;
    std::string callsign;   // the Osa's in the battery
};

class BatteryLink {
public:
    // How long the post counts the Osa as on a target it was given, before its crew has it (the message passed on, the
    // radar turned onto it; sam3d).
    static constexpr double kPendingS = 40.0;

    BatteryLink(ScenarioDef osa_def, ConsoleData osa_console, ScenarioDef post_def, ConsoleData post_console, std::string callsign)
        : callsign_(std::move(callsign)) {
        post_def.targets = osa_def.targets;  // one sky: the post's targets are the Osa's
        osa_ = std::make_unique<ScenarioRunner>(std::move(osa_def), std::move(osa_console));
        post_ = std::make_unique<ScenarioRunner>(std::move(post_def), std::move(post_console));
        post_->SetExternalLauncher(callsign_);
    }
    BatteryLink(const BatteryLink&) = delete;
    BatteryLink& operator=(const BatteryLink&) = delete;

    ScenarioRunner& Osa() { return *osa_; }
    const ScenarioRunner& Osa() const { return *osa_; }
    ScenarioRunner& Post() { return *post_; }
    const ScenarioRunner& Post() const { return *post_; }
    const std::string& Callsign() const { return callsign_; }
    double Time() const { return osa_->Time(); }
    // The Osa's scenario decides when it ends (its targets, its time limit).
    bool Finished() const { return osa_->Finished(); }

    void Step(double dt) {
        if (Finished()) return;
        osa_->Step(dt);
        post_->Step(dt);
        Sync();
    }
    bool RunToEnd(double dt, double max_s = 1e9) {
        while (!Finished() && Time() < max_s) Step(dt);
        return Finished();
    }

private:
    void Sync() {
        post_->MirrorTargets(*osa_);
        for (const ScenarioRunner::LinkedDesignation& d : post_->TakeLinkedDesignations()) {
            if (d.callsign != callsign_) continue;
            osa_->RelayDesignation(d.mark, d.start_s, d.end_s);
            pending_target_ = d.mark.target_id;
            pending_until_s_ = d.end_s + kPendingS;
        }
        osa_->SetPostNetUp(post_->PostBatteryNetUp());
        for (const ScenarioRunner::LinkReport& r : osa_->TakeOwnReports()) post_->HearBatteryReport(r);
        for (const ScenarioRunner::LinkReport& r : post_->TakeNetReports()) {
            if (r.kind == BatteryFire::Event::Kind::Hit) osa_->DestroyTarget(r.target, r.callsign);
            osa_->HearBatteryReport(r);
        }
        // The Osa in the post's fire distribution: on the target its crew tracks or was ordered to take, else the one the
        // post gave it a moment ago; free once that is destroyed or gone.
        std::string target = osa_->BatteryTarget();
        if (target.empty() && osa_->Time() < pending_until_s_) target = pending_target_;
        if (!target.empty() && !osa_->TargetAlive(target)) target.clear();
        const Launcher* l = osa_->GetLauncher();
        post_->SetExternalLauncherState(callsign_, l != nullptr ? l->LoadedCount() : -1, target, osa_->Tracking());
    }

    std::string callsign_;
    std::unique_ptr<ScenarioRunner> osa_;
    std::unique_ptr<ScenarioRunner> post_;
    std::string pending_target_;
    double pending_until_s_ = -1.0;
};

namespace detail {

class BatteryLinkLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, BatteryLinkDef& out) {
        if (!Keys(root, "", {"link_id", "name", "description", "osa", "post", "callsign", "source", "confidence", "notes"}) ||
            !Provenance(root, "") || !RequireString(root, "", "link_id", out.id) || !RequireString(root, "", "name", out.name) ||
            !OptionalString(root, "", "description", out.description) || !RequireString(root, "", "osa", out.osa_file) ||
            !RequireString(root, "", "post", out.post_file) || !RequireString(root, "", "callsign", out.callsign)) {
            return false;
        }
        return true;
    }
    using ConsoleLoader::Fail;
};

}  // namespace detail

// Load a link and both its scenarios. Null with `error` set when a file is missing or the two don't fit together.
inline std::unique_ptr<BatteryLink> LoadBatteryLinkFile(const std::string& path, const std::string& data_dir,
                                                        std::string* error = nullptr, BatteryLinkDef* def_out = nullptr) {
    auto fail = [&](const std::string& why) -> std::unique_ptr<BatteryLink> {
        if (error != nullptr) *error = path + ": " + why;
        return nullptr;
    };
    std::string text;
    if (!detail::ReadTextFile(path, text)) return fail("cannot open");
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) return fail(parse_error);
    BatteryLinkDef def;
    detail::BatteryLinkLoader loader;
    if (!loader.Load(root, def)) return fail(loader.Message());
    ScenarioDef osa, post;
    ConsoleData osa_console, post_console;
    std::string e;
    if (!LoadScenarioFile(data_dir + "/" + def.osa_file, data_dir, osa, osa_console, &e)) return fail("osa: " + e);
    if (!LoadScenarioFile(data_dir + "/" + def.post_file, data_dir, post, post_console, &e)) return fail("post: " + e);
    if (osa.battery_commander != "post") return fail("osa: its battery_commander must be \"post\"");
    if (!osa.has_battery || osa.tuning_file.empty()) return fail("osa: needs a battery and the engagement model");
    if (!post.has_post || !post.has_battery) return fail("post: its own unit must be the command post, with a battery");
    if (!post.targets.empty()) return fail("post: takes the Osa's targets, so has none of its own");
    bool launcher = false;
    for (const BatteryVehicleDef& v : post.battery_vehicles) {
        launcher = launcher || (v.id == def.callsign && v.kind == BatteryVehicleKind::Launcher);
    }
    if (!launcher) return fail("post: no launcher '" + def.callsign + "' in its battery");
    bool own = false;
    for (const BatteryVehicleDef& v : osa.battery_vehicles) own = own || (v.own && v.callsign == def.callsign);
    if (!own) return fail("osa: its own callsign in the battery must be '" + def.callsign + "'");
    if (std::fabs(osa.origin.lat_rad - post.origin.lat_rad) > 1e-12 || std::fabs(osa.origin.lon_rad - post.origin.lon_rad) > 1e-12 ||
        std::fabs(osa.origin.alt_m - post.origin.alt_m) > 1e-6) {
        return fail("the two scenarios need the same origin");
    }
    if (def_out != nullptr) *def_out = def;
    return std::make_unique<BatteryLink>(std::move(osa), std::move(osa_console), std::move(post), std::move(post_console), def.callsign);
}

}  // namespace simcore
