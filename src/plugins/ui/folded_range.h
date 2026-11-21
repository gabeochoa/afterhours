#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace afterhours {
namespace ui {

// A contiguous run of model rows, [begin, end), hidden behind one visible row.
struct FoldRange {
  size_t begin = 0;
  size_t end = 0;
  size_t size() const { return end > begin ? end - begin : 0; }
  bool operator==(const FoldRange &) const = default;
};

class FoldedRows {
public:
  FoldedRows() = default;
  FoldedRows(size_t model_count, std::vector<FoldRange> folds = {}) {
    set_model_count(model_count);
    set_folds(std::move(folds));
  }

  void set_model_count(size_t count) {
    model_count_ = count;
    rebuild();
  }
  size_t model_count() const { return model_count_; }

  void set_folds(std::vector<FoldRange> folds) {
    folds_ = std::move(folds);
    rebuild();
  }
  const std::vector<FoldRange> &folds() const { return folds_; }

  // Returns false when the range is empty or outside the model.
  bool fold(FoldRange range) {
    if (range.begin >= range.end || range.begin >= model_count_)
      return false;
    folds_.push_back(range);
    rebuild();
    return true;
  }

  // Removes the fold hiding `model_index` (or starting there). False if none.
  bool unfold_model(size_t model_index) {
    for (size_t i = 0; i < folds_.size(); i++) {
      if (model_index >= folds_[i].begin && model_index < folds_[i].end) {
        folds_.erase(folds_.begin() + static_cast<long>(i));
        rebuild();
        return true;
      }
    }
    return false;
  }

  // Removes the fold shown at visible row `visible`. False if not a fold row.
  bool unfold_visible(size_t visible) {
    if (!is_fold_row(visible))
      return false;
    folds_.erase(folds_.begin() + static_cast<long>(folds_before(visible)));
    rebuild();
    return true;
  }

  void unfold_all() {
    folds_.clear();
    rebuild();
  }

  size_t visible_count() const {
    return model_count_ - hidden_ + folds_.size();
  }

  bool is_folded_model(size_t model) const {
    const size_t k = fold_containing(model);
    return k < folds_.size();
  }

  bool is_fold_row(size_t visible) const {
    if (visible >= visible_count())
      return false;
    const size_t k = folds_before(visible);
    return k < folds_.size() && visible_start(k) == visible;
  }

  // The fold shown at visible row `visible`; empty range when not a fold row.
  FoldRange fold_at(size_t visible) const {
    return is_fold_row(visible) ? folds_[folds_before(visible)] : FoldRange{};
  }

  // Visible -> model: a fold row maps to its range's begin.
  size_t model_index(size_t visible) const {
    if (model_count_ == 0)
      return 0;
    visible = std::min(visible, visible_count() - 1);
    const size_t k = folds_before(visible);
    if (k < folds_.size() && visible_start(k) == visible)
      return folds_[k].begin;
    return visible - k + hidden_prefix_[k];
  }

  // Model -> visible: hidden rows map to the fold placeholder row.
  size_t visible_index(size_t model) const {
    if (model_count_ == 0)
      return 0;
    model = std::min(model, model_count_ - 1);
    const size_t k = fold_containing(model);
    if (k < folds_.size())
      return visible_start(k);
    size_t before = 0;
    while (before < folds_.size() && folds_[before].end <= model)
      before++;
    return model - hidden_prefix_[before] + before;
  }

private:
  size_t model_count_ = 0;
  std::vector<FoldRange> folds_;
  std::vector<size_t> hidden_prefix_{0};
  size_t hidden_ = 0;

  size_t visible_start(size_t k) const {
    return folds_[k].begin - hidden_prefix_[k] + k;
  }

  void rebuild() {
    std::vector<FoldRange> clean;
    for (auto r : folds_) {
      r.end = std::min(r.end, model_count_);
      if (r.begin < r.end)
        clean.push_back(r);
    }
    std::sort(clean.begin(), clean.end(), [](const FoldRange &a, const FoldRange &b) {
      return a.begin != b.begin ? a.begin < b.begin : a.end < b.end;
    });
    folds_.clear();
    for (auto r : clean) {
      if (!folds_.empty() && r.begin < folds_.back().end)
        folds_.back().end = std::max(folds_.back().end, r.end);
      else
        folds_.push_back(r);
    }
    hidden_prefix_.assign(folds_.size() + 1, 0);
    for (size_t k = 0; k < folds_.size(); k++)
      hidden_prefix_[k + 1] = hidden_prefix_[k] + folds_[k].size();
    hidden_ = hidden_prefix_.back();
  }

  size_t folds_before(size_t visible) const {
    size_t lo = 0, hi = folds_.size();
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      if (visible_start(mid) < visible)
        lo = mid + 1;
      else
        hi = mid;
    }
    return lo;
  }

  size_t fold_containing(size_t model) const {
    size_t lo = 0, hi = folds_.size();
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      if (folds_[mid].begin <= model)
        lo = mid + 1;
      else
        hi = mid;
    }
    if (lo == 0)
      return folds_.size();
    const size_t k = lo - 1;
    return model < folds_[k].end ? k : folds_.size();
  }
};
}
}
