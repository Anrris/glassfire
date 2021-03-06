
#ifndef CENTROID_H
#define CENTROID_H

#include "glassfire_base.h"

namespace glassfire{

//===================================
//--- Implementation of Centroid ----
//-----------------------------------
template<typename AxisType, size_t Dimension, typename FeatureInfo>
class GlassfireType<AxisType, Dimension, FeatureInfo>::Centroid: public RtreeFeature
{
public:
    typedef std::list<Centroid>                             CentroidList;
    typedef std::shared_ptr<CentroidList>                   CentroidListPtr;
    typedef typename CentroidList::iterator                 CentroidListIterator;
    typedef std::pair<RtreePoint, CentroidListIterator>     CentroidRtreeValue;
    typedef bgi::rtree<CentroidRtreeValue, bgi::linear<16>> CentroidRtree;

private:
    Rtree &             mRtree_ref;
    RtreeFeature_s &    mRtreeFeature_s_ref;
    AxisType            m_box_len;
    std::string         mCentroidKeyStr;

    AxisType            N_soft_k;
    AxisType            N_k;

    Matrix              mCmat;
    Matrix              mInvCmat;
    AxisType            mCmatDet;
    AxisType            m_diff = std::numeric_limits<AxisType>::max();
    AxisType            m_minimum_difference;

    size_t              m_occupation_count = 0;

    Feature_s           m_in_range_feature_s;
    std::vector<size_t> m_in_range_feature_s_index;
    AxisType            m_distance_to_nearest_data_point;

    const AxisType  _pi_ = 3.141592653589793;
public:
    Centroid(
        Rtree &             _Rtree_ref,
        RtreeFeature_s&     _RtreeFeature_s_ref,
        Feature             _Mean,
        AxisType            _box_len,
        std::string         _centroid_key_str
        ):      
            RtreeFeature        (_Mean),
            mRtree_ref          (_Rtree_ref),
            mRtreeFeature_s_ref (_RtreeFeature_s_ref),
            m_box_len           (_box_len),
            mCentroidKeyStr     (_centroid_key_str)
    {
        // Zerolize mCmat
        for (int idx_r = 0; idx_r < mCmat.rows(); idx_r++)
        for (int idx_c = 0; idx_c < mCmat.cols(); idx_c++)
            if(idx_r != idx_c) mCmat(idx_r, idx_c) = 0;
    }

    auto distance_to(RtreeFeature & other) -> AxisType {
        AxisType total_del_retval = 0;
        for(size_t i=0; i<this->getFeature().size(); i++){
            AxisType del = this->at(i) - other.at(i);
            total_del_retval += del * del;
        }
        return sqrt(total_del_retval);
    }
    auto distance_to(Feature & feature) -> AxisType {
        AxisType total_del_retval = 0;
        for(size_t i=0; i<feature.size(); i++){
            AxisType del = this->at(i) - feature[i];
            total_del_retval += del * del;
        }
        return sqrt(total_del_retval);
    }

    auto updateMean(AxisType _minimum_difference) -> AxisType {
        m_minimum_difference = _minimum_difference;

        std::vector<RtreeValue> result_s;
        mRtree_ref.query(
                bgi::intersects(this->createBox(m_box_len)), back_inserter(result_s)
        );

        m_occupation_count = result_s.size();

        Feature old_mean = this->getFeature();
        Feature new_mean(Dimension, 0);

        // Calculate mean from all points within region.
        for(auto & vpair: result_s){
            size_t index = vpair.second;
            for(size_t i=0 ; i<new_mean.size() ; i++){
                new_mean[i] += mRtreeFeature_s_ref[index].at(i) / result_s.size();
            }
        }

        m_diff = distance_to(new_mean);

        this->setFeature(new_mean);

        return m_diff;
    }

    auto count() -> size_t {return m_occupation_count;}
    auto getKeyStr() -> std::string {return mCentroidKeyStr;}
    auto needUpdateMean() -> bool { return m_diff > m_minimum_difference; }
    auto printMean() -> std::string {
        std::stringstream ss;
        for(size_t i=0 ; i<this->getFeature().size(); i++){
            ss << this->at(i) << " ";
        }
        return ss.str();
    }

    auto scoreOfFeature(const Feature &feature) -> AxisType {
        auto rowVec = feature_sub_mean_to_rowVector(feature);
        auto colVec = feature_sub_mean_to_colVector(feature);
        AxisType mahalanDistance = (rowVec * mInvCmat * colVec)(0, 0);
        AxisType result = (AxisType)exp( -0.5* mahalanDistance) / sqrt( pow(2 * _pi_, Dimension) * mCmatDet);
        return result;
    }

    auto count_in_range_feature_s(CentroidRtree & centroidRtreeRef, bool using_box_len = true) -> void {

        // Query the nearest Centroid
        // The centroid itself is included inside the centroidRtree(Ref)
        std::vector<CentroidRtreeValue> nearest_result;
        centroidRtreeRef.query(bgi::nearest((RtreePoint)*this, 2), std::back_inserter(nearest_result));

        AxisType query_box_len = m_box_len;
        if(!using_box_len){
            // Calculate the distance of nearest neighborhood.
            AxisType nearest_neighbor_distance = 0;
            for (auto &iter: nearest_result){
                nearest_neighbor_distance =
                    std::max(
                        this->distance_to( *iter.second),
                        nearest_neighbor_distance
                    );
            }
            query_box_len = nearest_neighbor_distance;
        }

        // Using nearest_neighbor_distance and create box to query the surrounding data points.
        std::vector<RtreeValue> result_s;
        mRtree_ref.query(
                bgi::intersects(this->createBox(query_box_len)), back_inserter(result_s)
        );

        // Refine the data points to be inside a circle of nearest_neighbor_distance.
        for (auto &it : result_s) {
            Feature feature = mRtreeFeature_s_ref[it.second].getFeature();
            m_in_range_feature_s_index.push_back(it.second);
            if (this->distance_to(feature) < query_box_len)
                m_in_range_feature_s.push_back(feature);
        }

        std::vector<RtreeValue> nearest_data_point;
        mRtree_ref.query(bgi::nearest((RtreePoint)*this, 1), std::back_inserter(nearest_data_point));

        Feature feature = mRtreeFeature_s_ref[nearest_data_point.front().second].getFeature();

        // TODO: Consider deprecate this variable.
        m_distance_to_nearest_data_point = this->distance_to(feature);
    }

    auto get_in_range_feature_s() -> const Feature_s &{ return m_in_range_feature_s; }

    auto updateCovariantMatrix(
            CentroidRtree & centroidRtreeRef,
            CentroidListPtr centroidListPtr,
            bool need_regularize_cmat = true) -> AxisType {

        Matrix tmpCmat, diffCmat;
        bool is_fresh_start = true;

        AxisType weight_discriminator_epsilon = 0.000001;

        auto iterate_parameters = [&]() {

            // Zerolize tmpCmat
            for (int idx_r = 0; idx_r < tmpCmat.rows(); idx_r++)
            for (int idx_c = 0; idx_c < tmpCmat.cols(); idx_c++)
                tmpCmat(idx_r, idx_c) = 0;

            AxisType p_denumerator = 0;

            // Create the weightList to discriminate the datapoint.
            std::list<AxisType> weightList;
            for (auto & feature : m_in_range_feature_s) {
                if(is_fresh_start){
                    weightList.push_back(1.0);
                }
                else{
                    weightList.push_back(scoreOfFeature(feature));
                }
                p_denumerator += weightList.back();
            }

            // Loop through all in-range-reatures (data points) to create the covariant matrix
            for (auto & feature : m_in_range_feature_s) {

                auto colMeanVec = feature_sub_mean_to_colVector(feature);
                auto weight = weightList.front()/p_denumerator;
                weightList.pop_front();

                for (int idx_r = 0; idx_r < tmpCmat.rows(); idx_r++)
                for (int idx_c = 0; idx_c < tmpCmat.rows(); idx_c++) {
                    if(is_fresh_start){
                        tmpCmat(idx_r, idx_c) += colMeanVec(idx_r, 0) * colMeanVec(idx_c, 0)/
                                                    m_in_range_feature_s.size();
                    }
                    else{
                        tmpCmat(idx_r, idx_c) += (weight/(weight + weight_discriminator_epsilon)) * 
                                                 colMeanVec(idx_r, 0) * colMeanVec(idx_c, 0)/
                                                    m_in_range_feature_s.size();
                    }
                }
            }

            diffCmat = mCmat - tmpCmat;

            mCmat = tmpCmat;
            mInvCmat = mCmat.inverse();
            mCmatDet = mCmat.determinant();
        };

        AxisType max_diff = 0;

        auto self_consistnt = [&](){
            iterate_parameters();

            if( is_fresh_start ){
                is_fresh_start = false;
                return;
            }

            max_diff = 0;
            for (int idx_r = 0; idx_r < tmpCmat.rows(); idx_r++)
            for (int idx_c = 0; idx_c < tmpCmat.cols(); idx_c++) {
                max_diff = std::max(max_diff, abs(diffCmat(idx_r, idx_c)) );
            }
        };

        do{
            self_consistnt();
        } while(max_diff > m_minimum_difference);

        return max_diff;
    }

    auto get_model(AxisType regularize = 0) -> ClusterModel {
        if (regularize < 0)
            throw std::runtime_error(":regularize: must be positive.");
        return ClusterModel(
                    this->getFeature(),
                    getCovariantMatrix(),
                    mCentroidKeyStr,
                    m_in_range_feature_s_index,
                    regularize
                    );
    }

    auto getCovariantMatrix() -> const Matrix & { return mCmat; }

private:
    auto feature_sub_mean_to_rowVector(const Feature &feature) -> RowVector {
        RowVector rowVector;
        for(size_t i=0; i<feature.size(); i++){
            rowVector(0,i) = feature[i] - this->at(i);
        }
        return rowVector;
    }
    auto feature_sub_mean_to_colVector(const Feature &feature) -> ColVector {
        ColVector colVector;
        for(size_t i=0; i<feature.size(); i++){
            colVector(i,0) = feature[i] - this->at(i);
        }
        return colVector;
    }
};
//-----------------------------------
//--- Implementation of Centroid ----
//===================================

}



#endif