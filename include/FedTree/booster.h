//
// Created by liqinbin on 12/11/20.
//

#ifndef FEDTREE_BOOSTER_H
#define FEDTREE_BOOSTER_H

#include <FedTree/objective/objective_function.h>
#include <FedTree/metric/metric.h>
// function_builder
#include <FedTree/Tree/tree_builder.h>
#include <FedTree/util/multi_device.h>
#include "FedTree/common.h"
#include "FedTree/syncarray.h"
#include "FedTree/Tree/tree.h"

//#include "row_sampler.h"

std::mutex mtx;

class Booster {
public:
    void init(const DataSet &dataSet, const GBMParam &param);

    void boost(vector<vector<Tree>> &boosted_model);

private:
    SyncArray<GHPair> gradients;
    std::unique_ptr<ObjectiveFunction> obj;
    std::unique_ptr<Metric> metric;
    SyncArray<float_type> y;
    std::unique_ptr<FunctionBuilder> fbuilder;
//    RowSampler rowSampler;
    GBMParam param;
    int n_devices;
};


void Booster::init(const DataSet &dataSet, const GBMParam &param) {
//    int n_available_device;
//    cudaGetDeviceCount(&n_available_device);
//    CHECK_GE(n_available_device, param.n_device) << "only " << n_available_device
//                                                 << " GPUs available; please set correct number of GPUs to use";
    this->param = param;

    //here
    fbuilder.reset(FunctionBuilder::create(param.tree_method));
    fbuilder->init(dataSet, param);
    obj.reset(ObjectiveFunction::create(param.objective));
    obj->configure(param, dataSet);
    metric.reset(Metric::create(obj->default_metric_name()));
    metric->configure(param, dataSet);

    n_devices = param.n_device;
    int n_outputs = param.num_class * dataSet.n_instances();
    gradients = SyncArray<GHPair>(n_outputs);
    y = SyncArray<float_type>(dataSet.n_instances());
    y.copy_from(dataSet.y.data(), dataSet.n_instances());
}


void Booster::boost(vector<vector<Tree>> &boosted_model) {
    TIMED_FUNC(timerObj);
    std::unique_lock<std::mutex> lock(mtx);

    //update gradients
    obj->get_gradient(y, fbuilder->get_y_predict(), gradients);

//    if (param.bagging) rowSampler.do_bagging(gradients);
    PERFORMANCE_CHECKPOINT(timerObj);
    //build new model/approximate function
    boosted_model.push_back(fbuilder->build_approximate(gradients));

    PERFORMANCE_CHECKPOINT(timerObj);
    //show metric on training set
    LOG(INFO) << metric->get_name() << " = " << metric->get_score(fbuilder->get_y_predict());
}

#endif //FEDTREE_BOOSTER_H
