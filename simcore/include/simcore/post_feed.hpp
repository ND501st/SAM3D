// What the command post's own instruments show, in one block (docs/PU12_DEVELOPMENT.md P6, P7): the IT-45's tracks on
// the scale the ФР block is at, the plotting board's recent marks (the grid glass over the tube) and the launchers'
// reports on the IT-1 / IT-2 boards. Where the simulation runs it comes from the scenario runner (PostFeedNow); on a
// network client from the player's knowledge stream (em::ViewExtras), so the cabin's IT-45 and IT boards draw the same
// picture on every machine. As scope_feed.hpp is for the Osa's scopes.
#pragma once

#include <string>
#include <vector>

#include "simcore/command_post.hpp"
#include "simcore/em/replication.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct PostFeed {
    bool post = false;                              // the player's vehicle is a command post
    double scale_m = 0.0;                           // the IT-45's scale (0: no post)
    Vec3 position;                                  // where the post puts itself (ENU)
    std::vector<CommandPost::ScopeMark> scope;      // the IT-45 now (fractions of the scale)
    std::vector<em::ViewBoardMark> glass;           // the board's marks of the last few minutes, oldest first
    std::vector<LauncherStatus> launchers;          // the IT boards
    double marker_x = 0.0;                          // the DT-2 marker (fractions of the scale, x east, y north)
    double marker_y = 0.0;
    int marked = -1;                                // the target the marker is on (-1 none)
    double marked_arrival_s = -1.0;                 // its time of arrival at the post (-1 not closing)
    std::string keypad;                             // the ДХ window: the target's number ("" none)
    bool keypad_designating = false;                // ЦУ pressed, waiting for the launcher's place
    std::string keypad_last;                        // the last designation ("05>22")
    std::vector<int> heard;                         // air-picture reports heard, not yet written (board numbers, oldest first)
};

// The board's targets as the commander reads them: each number's latest mark.
inline std::vector<em::ViewBoardMark> LatestMarks(const PostFeed& f) {
    std::vector<em::ViewBoardMark> out;
    for (const em::ViewBoardMark& m : f.glass) {
        bool found = false;
        for (em::ViewBoardMark& o : out) {
            if (o.number == m.number) {
                if (m.stamp >= o.stamp) o = m;
                found = true;
            }
        }
        if (!found) out.push_back(m);
    }
    return out;
}

// The feed as a network client gets it, from the knowledge stream's extras.
inline PostFeed FromViewExtras(const em::ViewExtras& x) {
    PostFeed f;
    f.post = x.post;
    if (!x.post) return f;
    f.scale_m = x.post_scale_m;
    f.position = x.post_position;
    for (const em::ViewPostTrack& t : x.post_scope) {
        CommandPost::ScopeMark m;
        m.number = t.number;
        m.x = t.x;
        m.y = t.y;
        m.vx = t.vx;
        m.vy = t.vy;
        m.zone = t.zone;
        m.friendly = t.friendly;
        m.group = t.group;
        m.primary = t.primary;
        f.scope.push_back(m);
    }
    f.glass = x.board;
    for (const em::ViewLauncher& l : x.launchers) {
        LauncherStatus s;
        s.callsign = l.callsign;
        s.state = static_cast<LauncherStatus::State>(l.state);
        s.missiles = l.missiles;
        s.target_number = l.target_number;
        f.launchers.push_back(s);
    }
    f.marker_x = x.post_marker_x;
    f.marker_y = x.post_marker_y;
    f.marked = x.post_marked;
    f.marked_arrival_s = x.post_marked_arrival_s;
    f.keypad = x.post_keypad;
    f.keypad_designating = x.post_keypad_designating;
    f.keypad_last = x.post_keypad_last;
    f.heard = x.post_heard;
    return f;
}

// And the other way: what the server puts into the stream.
inline void ToViewExtras(const PostFeed& f, em::ViewExtras& x) {
    x.post = f.post;
    if (!f.post) return;
    x.post_scale_m = f.scale_m;
    x.post_position = f.position;
    x.post_scope.clear();
    for (const CommandPost::ScopeMark& m : f.scope) {
        x.post_scope.push_back({m.number, m.x, m.y, m.vx, m.vy, m.zone, m.friendly, m.group, m.primary});
    }
    x.board = f.glass;
    x.launchers.clear();
    for (const LauncherStatus& s : f.launchers) {
        x.launchers.push_back({s.callsign, static_cast<int>(s.state), s.missiles, s.target_number});
    }
    x.post_marker_x = f.marker_x;
    x.post_marker_y = f.marker_y;
    x.post_marked = f.marked;
    x.post_marked_arrival_s = f.marked_arrival_s;
    x.post_keypad = f.keypad;
    x.post_keypad_designating = f.keypad_designating;
    x.post_keypad_last = f.keypad_last;
    x.post_heard = f.heard;
}

}  // namespace simcore
