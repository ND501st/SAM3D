// Track F, F5.3: the situation map the campaign screen draws (docs/fronts/05_WHAT_THE_PLAYER_SEES.md section 3).
//
// The screen between missions shows our side's knowledge only. Under the fronts (the ground war's feed on) that is the
// battery's regiment's situation map: the line of contact, our units and the enemy as reported to the regiment, the air
// defence staff's conclusion on the enemy's air front if one reached it, each mark with its date and age. This class keeps
// the layout and the post's cached map for a caller that holds the campaign, the net and the feed (Unreal's campaign
// subsystem); it reads the campaign only through SituationLayoutOf (the war's static data), never its state. With the
// fronts off it gives no map, and the screen stays as it was.
#pragma once

#include <string>

#include "simcore/command/air_war_feed.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/command/situation_map.hpp"

namespace simcore {
namespace command {

class CampaignSituation {
public:
    // The regiment's post (the feed's ground role) has a map under the fronts; null with the fronts off or the post not in
    // the chain. The reference holds until the next ask after a change.
    const SituationMap* RegimentMap(const airwar::AirCampaign& c, const InfoNet& net, const CampaignFeed& feed, const OwnAirDef* own_air = nullptr) {
        return MapFor(c, net, feed, feed.Ground().Roles().regiment, own_air);
    }

    // The map of any post of the chain (a developer's view asks for the army's), under the fronts.
    const SituationMap* MapFor(const airwar::AirCampaign& c, const InfoNet& net, const CampaignFeed& feed, const std::string& post_id,
                               const OwnAirDef* own_air = nullptr) {
        if (!feed.Ground().On()) return nullptr;
        const int post = net.Data().PositionIndex(post_id);
        if (post < 0) return nullptr;
        // The layout is of the war's static data; the units' plans are in it once the strategy is on (decided at the first pump).
        const bool ad_on = feed.Ad().On();
        if (!have_layout_ || ad_on != layout_ad_on_) {
            layout_ = SituationLayoutOf(c, &feed.Ad(), own_air);
            have_layout_ = true;
            layout_ad_on_ = ad_on;
            maps_.Clear();
        }
        return &maps_.For(net, feed, layout_, post);
    }

    // The campaign, net or feed was replaced: forget the layout and the maps.
    void Reset() {
        have_layout_ = false;
        layout_ad_on_ = false;
        maps_.Clear();
    }

    const SituationLayout& Layout() const { return layout_; }
    int Builds() const { return maps_.Builds(); }

private:
    SituationLayout layout_;
    SituationMaps maps_;
    bool have_layout_ = false;
    bool layout_ad_on_ = false;
};

}  // namespace command
}  // namespace simcore
