#include "sphere_finding.h"

using namespace std;
using namespace Eigen;
using namespace sensor_msgs;

// if this function returns true,
// hit1_axis1 * plane_axis1 + hit1_axis2 * plane_axis2 and
// hit2_axis1 * plane_axis1 + hit2_axis2 * plane_axis2
// will be the two points on the intersection of the plane and the sphere
// that are tangent to the origin
bool intersect_plane_sphere(Vector3d plane_axis1, Vector3d plane_axis2,
        Vector3d sphere_pos, double sphere_radius,
        double &hit1_axis1, double &hit1_axis2,
        double &hit2_axis1, double &hit2_axis2) {
    
    // orthonormalize plane axes. z axis is normal to plane
    Matrix3d real_from_flat; real_from_flat << // fill columns
        plane_axis1.normalized(),
        plane_axis1.cross(plane_axis2).cross(plane_axis1).normalized(),
        plane_axis1.cross(plane_axis2).normalized();
    Matrix3d flat_from_real = real_from_flat.inverse();
    
    Vector3d sphere_pos_flat = flat_from_real * sphere_pos;
    // now sphere_pos_plane has the closest point to the center in 
    // flat coordinates in [0] and [1] and abs(sphere_pos_plane[2])
    // is the distance from the plane to the sphere
    
    if(abs(sphere_pos_flat[2]) > sphere_radius)
        return false; // plane doesn't intersect sphere
    
    double r = sqrt(pow(sphere_radius, 2) - pow(sphere_pos_flat(2), 2)); // radius of intersection (circle)
    double cx = sphere_pos_flat(0);
    double cy = sphere_pos_flat(1);
    
    double dist = cx*cx + cy*cy - r*r;
    if(dist < 0)
        return false; // origin is within sphere
    double inner = r*sqrt(dist);
    double cx2_plus_cy2 = cx*cx + cy*cy;
    Vector3d hit1_flat(
        (cx*cx*cx + cy*cy*cx - cx*r*r - cy*inner)/cx2_plus_cy2,
        (cy*cy*cy + cx*cx*cy - cy*r*r + cx*inner)/cx2_plus_cy2,
        0);
    Vector3d hit2_flat(
        (cx*cx*cx + cy*cy*cx - cx*r*r + cy*inner)/cx2_plus_cy2, 
        (cy*cy*cy + cx*cx*cy - cy*r*r - cx*inner)/cx2_plus_cy2,
        0);
    
    Matrix3d real_from_plane; real_from_plane <<
        plane_axis1,
        plane_axis2,
        plane_axis1.cross(plane_axis2).normalized();
    Matrix3d plane_from_real = real_from_plane.inverse();
    
    Matrix3d plane_from_flat = plane_from_real * real_from_flat;
    
    Vector3d hit1_plane = plane_from_flat * hit1_flat;
    hit1_axis1 = hit1_plane(0); hit1_axis2 = hit1_plane(1);
    assert(abs(hit1_plane(2)) < 1e-3);
    
    Vector3d hit2_plane = plane_from_flat * hit2_flat;
    hit2_axis1 = hit2_plane(0); hit2_axis2 = hit2_plane(1);
    assert(abs(hit1_plane(2)) < 1e-3);
    
    return true;
}

bool _accumulate_sphere_scanline(const vector<Vector3d>& sumimage, const CameraInfoConstPtr& cam_info, Vector3d sphere_pos_camera, double sphere_radius, const Matrix<double, 3, 4> &proj, uint32_t yy, Vector3d &total_color, double &count, vector<int>* dbg_image=NULL) {
    double y = yy;
    
    // solution space of project((X, Y, Z)) = (x, y, z) with y fixed
    Vector3d plane1(
        0,
        -((cam_info->P[7] + cam_info->P[6]) - y*(cam_info->P[11] + cam_info->P[10]))/(cam_info->P[ 5] - y*cam_info->P[9]),
        1);
    Vector3d plane2(
        1,
        (y*cam_info->P[8] - cam_info->P[4])/(cam_info->P[5] - y*cam_info->P[9]),
        0);
    
    double hit1_axis1, hit1_axis2;
    double hit2_axis1, hit2_axis2;
    if(!intersect_plane_sphere(plane1, plane2, sphere_pos_camera, sphere_radius, hit1_axis1, hit1_axis2, hit2_axis1, hit2_axis2)) {
        // sphere doesn't intersect scanline
        //cout << yy << " miss" << endl;
        return false;
    }
    
    Vector3d point1 = hit1_axis1 * plane1 + hit1_axis2 * plane2;
    Vector3d point2 = hit2_axis1 * plane1 + hit2_axis2 * plane2;
    
    Vector3d point1_homo = proj * point1.homogeneous();
    if(point1_homo(2) <= 0)
        return true; // behind camera
    Vector2d point1_screen = point1_homo.hnormalized();
    assert(abs(point1_screen(1) - y) < 1e-3);
    
    Vector3d point2_homo = proj * point2.homogeneous();
    if(point2_homo(2) <= 0)
        return true; // behind camera
    Vector2d point2_screen = point2_homo.hnormalized();
    assert(abs(point2_screen(1) - y) < 1e-3);
    
    double minx = min(point1_screen(0), point2_screen(0));
    double maxx = max(point1_screen(0), point2_screen(0));
    
    int xstart = max(0, min((int)cam_info->width, (int)ceil(minx)));
    int xend   = max(0, min((int)cam_info->width, (int)ceil(maxx))); // not inclusive
    
    total_color += (xend == 0 ? Vector3d::Zero() : sumimage[yy * cam_info->width + xend - 1]) - (xstart == 0 ? Vector3d::Zero() : sumimage[yy * cam_info->width + xstart - 1]);
    count += xend - xstart;
    if(dbg_image) {
        for(int X = xstart; X < xend; X++) {
            (*dbg_image)[yy * cam_info->width + X] += 1;
        }
    }
    
    /*cout << yy << " "
        << "(" << point1_screen(0) << " " << point1_screen(1) << ") "
        << "(" << point2_screen(0) << " " << point2_screen(1) << ")" << endl;*/
    
    return true;
}

void sphere_query(const vector<Vector3d>& sumimage, const CameraInfoConstPtr& cam_info, Vector3d sphere_pos_camera, double sphere_radius, Vector3d &total_color, double &count, vector<int>* dbg_image) {
    // get the sum of the color values (along with the count) of the pixels
    // that are included within the provided sphere specified by
    // sphere_pos_camera and sphere_radius
    
    Matrix<double, 3, 4> proj;
    for(int row = 0; row < 3; row++)
        for(int col = 0; col < 4; col++)
            proj(row, col) = cam_info->P[4*row + col];
    
    Vector2d center_screen = (proj * sphere_pos_camera.homogeneous()).eval().hnormalized();
    int32_t y_center_hint = max(min(center_screen(1), (double)cam_info->height-1), 0.) + .5;
    
    total_color = Vector3d(0, 0, 0);
    count = 0;
    
    for(int32_t yy = y_center_hint; yy >= 0; yy--) {
        if(!_accumulate_sphere_scanline(sumimage, cam_info, sphere_pos_camera, sphere_radius, proj, yy, total_color, count, dbg_image))
            break;
    }
    for(int32_t yy = y_center_hint + 1; yy < (int32_t)cam_info->height; yy++) {
        if(!_accumulate_sphere_scanline(sumimage, cam_info, sphere_pos_camera, sphere_radius, proj, yy, total_color, count, dbg_image))
            break;
    }
}
