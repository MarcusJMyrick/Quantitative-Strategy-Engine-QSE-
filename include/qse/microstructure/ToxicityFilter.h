#pragma once

namespace qse {

/// What to do with a marketable order given the current flow toxicity.
enum class ExecAction {
    Cross,       ///< take liquidity now (blind market order)
    RestPassive, ///< rest at the near touch and wait (avoid crossing into toxic flow)
};

/**
 * @class ToxicityFilter
 * @brief VPIN + OFI gate for whether to cross the spread now or rest passive.
 *
 * QR1.3. Crossing the spread into toxic, one-sided flow is where an uninformed
 * taker gets picked off. This filter rests passive (delays crossing) only when
 * flow is BOTH toxic (VPIN above threshold) AND directionally favorable to a
 * passive order — a buy wants net *selling* pressure (OFI < 0) to come hit its
 * resting bid; a sell wants net *buying* (OFI > 0). Gating on direction as well
 * as toxicity is deliberate: resting passive into *same-direction* toxic flow
 * would simply miss the fill and fall back worse, so those orders (and all
 * benign flow) cross immediately. Whether the filter nets out ahead is settled
 * empirically by the A/B audit (docs/research/execution/).
 */
class ToxicityFilter {
public:
    explicit ToxicityFilter(double vpin_threshold = 0.4) : vpin_threshold_(vpin_threshold) {}

    ExecAction decide(bool is_buy, double vpin, double ofi) const {
        const bool toxic = vpin > vpin_threshold_;
        const bool favorable = is_buy ? (ofi < 0.0) : (ofi > 0.0);
        return (toxic && favorable) ? ExecAction::RestPassive : ExecAction::Cross;
    }

    double vpin_threshold() const { return vpin_threshold_; }

private:
    double vpin_threshold_;
};

} // namespace qse
