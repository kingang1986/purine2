// Copyright Lin Min 2015
#include "composite/layer.hpp"
#include "dispatch/blob.hpp"
#include "composite/graph/copy.hpp"
#include "composite/vectorize.hpp"

namespace purine {

typedef vector<Blob*> B;

Layer::Layer(int rank, int device, const vector<Blob*>& weight)
    : Connectable(rank, device) {
  weight_ = weight;
}

Layer::~Layer() {}

const vector<Blob*>& Layer::weight() const {
  CHECK(bottom_setup_);
  CHECK(top_setup_);
  return weight_;
}

const vector<Blob*>& Layer::loss() const {
  CHECK(bottom_setup_);
  CHECK(top_setup_);
  return loss_;
}

vector<Blob*> Layer::weight_data() {
  return first_half<Blob*>(weight_);
}

vector<Blob*> Layer::weight_diff() {
  return second_half<Blob*>(weight_);
}

vector<Blob*> Layer::weight(int index) {
  CHECK_EQ(weight_.size() % 2, 0);
  CHECK_LT(index, weight_.size() / 2);
  int middle = weight_.size() / 2;
  return { weight_[index], weight_[middle + index] };
}

vector<Blob*> Layer::bottom_data() {
  return first_half<Blob*>(bottom_);
}

vector<Blob*> Layer::bottom_diff() {
  return second_half<Blob*>(bottom_);
}

vector<Blob*> Layer::top_data() {
  return first_half<Blob*>(top_);
}

vector<Blob*> Layer::top_diff() {
  return second_half<Blob*>(top_);
}

vector<Blob*> Layer::bottom(int index) {
  CHECK_EQ(bottom_.size() % 2, 0);
  CHECK_LT(index, bottom_.size() / 2);
  int middle = bottom_.size() / 2;
  return { bottom_[index], bottom_[middle + index] };
}

vector<Blob*> Layer::top(int index) {
  CHECK_EQ(top_.size() % 2, 0);
  CHECK_LT(index, top_.size() / 2);
  int middle = top_.size() / 2;
  return { top_[index], top_[middle + index] };
}

Layer& operator >> (Layer& layer1, Layer& layer2) {
  if (layer1.rank() == layer2.rank() && layer1.device() == layer2.device()) {
    layer2.set_bottom(layer1.top());
    return layer2;
  } else {
    return layer1.top() >> layer2;
  }
}

Layer& operator >> (const vector<Blob*>& bottom, Layer& layer) {
  // Copyup
  vector<Blob*> bottom_data = first_half<Blob*>(bottom);
  vector<Blob*> bottom_diff = second_half<Blob*>(bottom);
  auto vec_copy = layer.createGraph<Vectorize<Copy> >("...",
      vector<int>(bottom_data.size(), layer.rank()),
      vector<int>(bottom_data.size(), layer.device()), true);
  vector<B>{ bottom_data } >> *vec_copy;
  // Copydown
  vector<Blob*> bottom_diff_tmp(bottom_diff.size());
  transform(bottom_diff.begin(), bottom_diff.end(), bottom_diff_tmp.begin(),
      [&](Blob* b)->Blob* {
        if (b->rank() == layer.rank() && b->device() == layer.device()) {
          return b;
        } else {
          Blob* tmp = layer.create(b->tensor()->size(), "...",
              layer.rank(), layer.device());
          B{ tmp } >> *layer.createGraph<Copy>("...") >> B{ b };
          return tmp;
        }
      });
  // set bottom
  vector<Blob*> bottom_tmp = vec_copy->top()[0];
  bottom_tmp.insert(bottom_tmp.end(), bottom_diff_tmp.begin(),
      bottom_diff_tmp.end());
  layer.set_bottom(bottom_tmp);
  return layer;
}

const vector<Blob*>& operator >> (Layer& layer, const vector<Blob*>& top) {
  // Copyup
  vector<Blob*> top_data = first_half<Blob*>(top);
  vector<Blob*> top_diff = second_half<Blob*>(top);
  vector<Blob*> top_data_tmp(top_data.size());
  transform(top_data.begin(), top_data.end(), top_data_tmp.begin(),
      [&](Blob* b)->Blob* {
        if (b->rank() == layer.rank() && b->device() == layer.device()) {
          return b;
        } else {
          Blob* tmp = layer.create(b->tensor()->size(), "...",
              layer.rank(), layer.device());
          B{ tmp } >> *layer.createGraph<Copy>("...") >> B{ b };
          return tmp;
        }
      });
  // Copydown
  Vectorize<Copy>* vec_copy = layer.createGraph<Vectorize<Copy> >("...",
      vector<int>(top_diff.size(), layer.rank()),
      vector<int>(top_diff.size(), layer.device()), true);
  vector<B>{ top_diff } >> *vec_copy;
  // set top
  vector<Blob*> top_tmp = std::move(top_data_tmp);
  vector<Blob*> top_diff_tmp = vec_copy->top()[0];
  top_tmp.insert(top_tmp.end(), top_diff_tmp.begin(), top_diff_tmp.end());
  layer.set_top(top_tmp);
  return top;
}

}