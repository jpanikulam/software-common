#ifndef IFINDER_H
#define IFINDER_H

#include <utility>
#include <vector>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

#include <opencv/cv.h>

#include <ImageSource.h>

class IFinder {
	public:
		struct FinderResult {
			FinderResult(const std::vector<boost::property_tree::ptree> &results, const cv::Mat &res, const cv::Mat &dbg) : results(results), res(res), dbg(dbg) { };
			const std::vector<boost::property_tree::ptree> results;
			const cv::Mat res;
			const cv::Mat dbg;
		};
		IFinder(std::vector<std::string> objectPath, boost::property_tree::ptree config) {
			this->objectPath = objectPath;
			this->config = config;
		}
		std::vector<std::string> objectPath;
		virtual FinderResult find(const subjugator::ImageSource::Image &img) = 0;
	protected:
		boost::property_tree::ptree config;
		boost::property_tree::ptree Point_to_ptree(const cv::Point& p, const subjugator::ImageSource::Image &image) {
		    cv::Point3d ray = image.camera_model.projectPixelTo3dRay(p);
			boost::property_tree::ptree result;
			result.push_back(std::make_pair("", boost::lexical_cast<std::string>(ray.x)));
			result.push_back(std::make_pair("", boost::lexical_cast<std::string>(ray.y)));
			result.push_back(std::make_pair("", boost::lexical_cast<std::string>(ray.z)));
			return result;
		}
};

#endif
