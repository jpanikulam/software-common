#include <boost/foreach.hpp>

#include <stdio.h>
#include <iostream>

#include "Contours.h"
#include "Normalizer.h"
#include "Thresholder.h"

#include "ShooterFinder.h"

using namespace boost;
using namespace cv;
using namespace std;

IFinder::FinderResult ShooterFinder::find(const subjugator::ImageSource::Image &img) {
	Thresholder thresholder(img.image);

	// call to thresholder here
	Mat dbg = thresholder.simpleHSV(config.get<uchar>(objectPath[0] + "_hue"),
					config.get<uchar>(objectPath[0] + "_hue_range"),
					config.get<uchar>(objectPath[0] + "_sat_C"));

	erode(dbg,dbg,cv::Mat::ones(5,5,CV_8UC1));
	dilate(dbg,dbg,cv::Mat::ones(5,5,CV_8UC1));

	// call to specific member function here
	Contours contours(dbg, 50, 7000000,1500000);

	// Draw result
	Mat res = img.image.clone();
	contours.drawResult(res, CV_RGB(255, 255, 255));

	vector<property_tree::ptree> resultVector;
	if(objectPath[1] == "box") {
		// Prepare results
		if(contours.boxes.size() && contours.shapes.size()) {
			property_tree::ptree fResult;
			fResult.put_child("center", Point_to_ptree(contours.boxes[0].centroid, img));
			fResult.put("scale", contours.boxes[0].area);
			fResult.put("angle", contours.boxes[0].orientationError);
			resultVector.push_back(fResult);
		}
	} else if(objectPath[1] == "small") {
		Contours::InnerContour bestShape;
		bool foundSomething = false;
		BOOST_FOREACH(const Contours::InnerContour &shape, contours.shapes)
			if(shape.circularity > 0.5 && shape.area < shape.outer_area*2/3 && (!foundSomething || shape.area > bestShape.area)) {
				foundSomething = true;
				bestShape = shape;
			}
		if(foundSomething) {
			Contours::InnerContour bestShape2;
			{
				bool foundSomething2 = false;
				BOOST_FOREACH(const Contours::InnerContour &shape, contours.shapes)
					if(shape.circularity > 0.5 && shape.area < shape.outer_area*2/3 && (!foundSomething2 || shape.area > bestShape2.area) && shape.area < bestShape.area) {
						foundSomething2 = true;
						bestShape2 = shape;
					}
				if(!foundSomething2)
					bestShape2 = bestShape;
			}
			if(bestShape2.area < 0.1*bestShape.area) // if second largest is an order of magnitude smaller
				bestShape2 = bestShape; // use largest

			circle(res, bestShape2.centroid, 10, CV_RGB(255, 255, 0), 2);

			property_tree::ptree fResult;
			fResult.put_child("center", Point_to_ptree(bestShape2.centroid, img));
			//fResult.put("angle", contours.boxes[0].orientationError);
			fResult.put("scale", bestShape2.area);
			resultVector.push_back(fResult);
		}
	} else { assert(objectPath[1] == "large");
		if(contours.shapes.size()) {
			Contours::InnerContour shape = contours.findLargestShape();
			property_tree::ptree fResult;
			fResult.put_child("center", Point_to_ptree(shape.centroid, img));
			//fResult.put("angle", contours.boxes[0].orientationError);
			fResult.put("scale", shape.area);
			resultVector.push_back(fResult);
		}
	}
	return FinderResult(resultVector, res, dbg);
}
