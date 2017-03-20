// This file is part of MLDB. Copyright 2017 mldb.ai inc. All rights reserved.

/* onevsall_generator.cc                                          -*- C++ -*-
   Mathieu Marquis Bolduc, 8 March 2017
   Copyright (c) 2017 MLDB.ai  All rights reserved.
   $Source$

   Generator for onevsall classifiers
*/

#include "onevsall_generator.h"
#include "mldb/ml/jml/registry.h"
#include "mldb/jml/utils/smart_ptr_utils.h"

#include "training_index.h"

using namespace std;


namespace ML {

/*****************************************************************************/
/* ONEVSALL_GENERATOR                                                        */
/*****************************************************************************/

OneVsAll_Classifier_Generator::
OneVsAll_Classifier_Generator()
{
    defaults();
}

OneVsAll_Classifier_Generator::
OneVsAll_Classifier_Generator(std::shared_ptr<Classifier_Generator> learner)
{
    defaults();
    weak_learner = learner;
}

OneVsAll_Classifier_Generator::~OneVsAll_Classifier_Generator()
{
}

void
OneVsAll_Classifier_Generator::
configure(const Configuration & config, vector<string> & unparsedKeys)
{
    Classifier_Generator::configure(config, unparsedKeys);

    config.find(verbosity, "verbosity");

    weak_learner = get_trainer("weak_learner", config);
}

void
OneVsAll_Classifier_Generator::
defaults()
{
    Classifier_Generator::defaults();
    weak_learner.reset();
}

Config_Options
OneVsAll_Classifier_Generator::
options() const
{
    Config_Options result = Classifier_Generator::options();
    result       
        .subconfig("weak_leaner", weak_learner,
                   "Binary classifier for each label value");
    
    return result;
}

void
OneVsAll_Classifier_Generator::
init(std::shared_ptr<const Feature_Space> fs, Feature predicted)
{
    Classifier_Generator::init(fs, predicted);
    model = OneVsAllClassifier(fs, predicted);    
}

std::shared_ptr<Classifier_Impl>
OneVsAll_Classifier_Generator::
generate(Thread_Context & context,
         const Training_Data & training_data,
         const distribution<float> & weights,
         const std::vector<Feature> & features,
         int) const
{
    std::shared_ptr<OneVsAllClassifier> current = make_shared<OneVsAllClassifier>(model);

    ML::Mutable_Feature_Info labelInfo = ML::Mutable_Feature_Info(ML::BOOLEAN);

    int labelValue = 0; 

    std::shared_ptr<Mutable_Feature_Space> fs2 = std::dynamic_pointer_cast<Mutable_Feature_Space>(make_sp(feature_space->make_copy()));
    ExcAssert(fs2);
    fs2->set_info(predicted, labelInfo);

    std::shared_ptr<Training_Data> mutable_trainingData(make_sp(training_data.make_copy()));
    ExcAssert(mutable_trainingData->example_count() == training_data.example_count());
    mutable_trainingData->set_feature_space(fs2);

    if (verbosity > 0)
        cerr << numUniqueLabels << " unique label" << endl;

    do {        

        if (verbosity > 0)
            cerr << "label " << labelValue << " out of " << numUniqueLabels << endl;

        //Fix the feature space and training_data for binary classification
        weak_learner->init(fs2, predicted);

        for(size_t i = 0; i < mutable_trainingData->example_count(); ++i) {

           float currentLabelValue = training_data[i][predicted];
           int combinaisonIndex = (int)currentLabelValue;
           ExcAssert(combinaisonIndex < multiLabelList.size());

           const auto& combinaison = multiLabelList[combinaisonIndex];
           float new_value = std::count(combinaison.begin(), combinaison.end(), labelValue) > 0 ? 1.0f : 0.0f;
           mutable_trainingData->modify_feature(i, predicted, new_value);
        }

        auto subClassifier = weak_learner->generate(context, *mutable_trainingData, weights, features);

        current->push(subClassifier);

        ++labelValue;
    } while (labelValue < numUniqueLabels);

    return current;
}

/*****************************************************************************/
/* REGISTRATION                                                              */
/*****************************************************************************/

namespace {

Register_Factory<Classifier_Generator, OneVsAll_Classifier_Generator>
    NULL_CLASSIFIER_REGISTER("onevsall");

} // file scope

} // namespace ML