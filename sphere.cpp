#include "sphere.h"
#include "field.h"
#include "vector2D.h"
#include "ray.h"
#include "common_functions.h"
#include <QtMath>

sphere::sphere(vector2D cntr, qreal r,
			   qreal index_of_refraction, field *backg)
	:abstract_optics(backg)
{
	center = cntr;
	radius = r;
	index_of_refr = index_of_refraction;

	setup_pen_and_bruh();
	generate_outline();
}

sphere::~sphere() {}

vector2D *sphere::intersection_with_ray(ray *r) const
{
	return common_functions::intersection_with_circle
			(center, radius, r);
}

ray *sphere::generate_ray(ray *r)
{
	if (r->get_intensity() < ray::min_intensity) return 0;
	if (r->get_intersection_object() != this) return 0;
	vector2D cross = *r->get_intersection_point();

	vector2D n = (cross - center) / radius;

	return common_functions::generate_prism_ray
			(cross, n, index_of_refr, r, background);
}

void sphere::generate_outline()
{
	outline.addEllipse(center, radius, radius);
}