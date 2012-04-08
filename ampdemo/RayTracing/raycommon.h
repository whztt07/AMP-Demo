#pragma once

#include <amp.h>
#include <amp_math.h>
#include "ampvectors.h"

template <typename fp_t>
class intersect_result
{
public:
	vector3<fp_t> position;
	vector3<fp_t> normal;
	fp_t distance;
	int material;
	bool is_hit;

	intersect_result() restrict(cpu, amp) : is_hit(false), material(0), distance(0.0f) {}
	intersect_result(const intersect_result& other) restrict(cpu, amp) 
		: is_hit(other.is_hit), material(other.material), distance(other.distance), position(other.position), normal(other.normal) {}
	explicit intersect_result(bool is_hit, int material, fp_t distance, const vector3<fp_t>& position, const vector3<fp_t>& normal) restrict(cpu, amp) 
		: is_hit(is_hit), material(material), distance(distance), position(position), normal(normal) {}
};


template <typename fp_t>
class ray
{
public:
	vector3<fp_t> origin;
	vector3<fp_t> direction;

	explicit ray(const vector3<fp_t>& origin, const vector3<fp_t>& direction) restrict(cpu, amp) : origin(origin), direction(direction) {}
	ray(const ray& other) restrict(cpu, amp) : origin(other.origin), direction(other.direction) {}

	vector3<fp_t> get_point(fp_t t) const restrict(cpu, amp)
	{
		return origin + (direction * t);
	}
};

template <typename fp_t>
class perspective_camera
{
public:
	vector3<fp_t> eye;
	vector3<fp_t> front;
	vector3<fp_t> right;
	vector3<fp_t> up;
	fp_t fov_scale;

	explicit perspective_camera(vector3<fp_t> eye, vector3<fp_t> front, vector3<fp_t> ref_up, float fov) restrict(cpu, amp) 
		: eye(eye), front(front)
	{
		right = front.cross(ref_up);
		up = right.cross(front);
		fov_scale = math_helper<fp_t>::tan(fov * 0.5f * 3.141593f / 180.0f) * 2.0f;
	}

	ray<fp_t> generate_ray(fp_t x, fp_t y) const restrict(cpu, amp)
	{
		vector3<fp_t> r(right * ((x - 0.5f) * fov_scale));
		vector3<fp_t> u(up * ((y - 0.5f) * fov_scale));

		return ray<fp_t>(eye, (front + r + u).normalize());
	}
};

template <typename fp_t>
class color
{
public:
	fp_t r;
	fp_t g;
	fp_t b;

	explicit color(fp_t r, fp_t g, fp_t b) restrict(cpu, amp) : r(r), g(g), b(b) {}
	color(const color& other) restrict(cpu, amp) : r(other.r), g(other.g), b(other.b) {}

	color operator+(const color& c) const restrict(cpu, amp)
	{
		return color(r + c.r, g + c.g, b + c.b);
	}

	color operator*(fp_t s) const restrict(cpu, amp)
	{
		return color(r * s, g * s, b * s);
	}

	color operator*(const color& c) const restrict(cpu, amp)
	{
		return color(r * c.r, g * c.g, b * c.b);
	}

	static color black() restrict(cpu, amp) { return color(0.0f, 0.0f, 0.0f); }
	static color white() restrict(cpu, amp) { return color(1.0f, 1.0f, 1.0f); }
	static color red() restrict(cpu, amp) { return color(1.0f, 0.0f, 0.0f); }
	static color green() restrict(cpu, amp) { return color(0.0f, 1.0f, 0.0f); }
	static color blue() restrict(cpu, amp) { return color(0.0f, 0.0f, 1.0f); }
};

class material
{
public:
	enum material_type
	{
		material_checker,
		material_phong
	};

	template<typename fp_t> 
	color<fp_t> sample(ray<fp_t> ray, vector3<fp_t> position, vector3<fp_t> normal) const restrict(cpu, amp)
	{
		switch (type)
		{
		case material_checker:
			return static_cast<const checker<fp_t>*>(this)->sample_impl(ray, position, normal);
		case material_phong:
			return static_cast<const phong<fp_t>*>(this)->sample_impl(ray, position, normal);
		default:
			return color<fp_t>::black();
		}
	}
protected:
	explicit material(int type) restrict(cpu, amp) : type(type) {}

private:
	int type;

};

template <typename fp_t>
class checker : public material
{
public:
	checker(fp_t scale, fp_t reflectiveness) restrict(cpu, amp) : material(material_checker), scale(scale), reflectiveness(reflectiveness) {}

	color<fp_t> sample_impl(ray<fp_t> ray, vector3<fp_t> position, vector3<fp_t> normal)const restrict(cpu, amp)
	{
		fp_t r = math_helper<fp_t>::fabs(math_helper<fp_t>::floor(position.x * scale) + math_helper<fp_t>::floor(position.z * scale));

		return (static_cast<int>(r) % 2) < 1 ? color<fp_t>::black() : color<fp_t>::white();
	}
private:
	fp_t scale;
	fp_t reflectiveness;
};

template <typename fp_t>
class phong : public material
{
public:
	phong(color<fp_t> diffuse, color<fp_t> specular, fp_t shininess, fp_t reflectiveness)
		: material(material_phong), diffuse(diffuse), specular(specular), shininess(shininess), reflectiveness(reflectiveness) {}

	color<fp_t> sample_impl(ray<fp_t> ray, vector3<fp_t> position, vector3<fp_t> normal)const restrict(cpu, amp)
	{
		vector3<fp_t> light_dir(0.5773503f, 0.5773503f, 0.5773503f);
		color<fp_t> light_color(color<fp_t>::white());

		fp_t n_dot_l = normal.dot(light_dir);
		vector3<fp_t> h((light_dir - ray.direction).normalize());
		fp_t n_dot_h = normal.dot(h);
		
		color<fp_t> diffuse_term = diffuse * math_helper<fp_t>::fmax(n_dot_l, 0.0f);
		color<fp_t> specular_term = specular * math_helper<fp_t>::pow(math_helper<fp_t>::fmax(n_dot_h, 0.0f), shininess);

		return light_color * (diffuse_term + specular_term);
	}

private:
	color<fp_t> diffuse;
	color<fp_t> specular;
	fp_t shininess;
	fp_t reflectiveness;
};

template <typename fp_t>
class sphere
{
public:
	vector3<fp_t> center;
	fp_t radius;

	explicit sphere(const vector3<fp_t>& center, fp_t radius) restrict(cpu, amp) : center(center), radius(radius) { init(); }
	sphere(const sphere& other) restrict(cpu, amp) : center(other.center), radius(other.radius) { init(); }

	intersect_result<fp_t> intersect(const ray<fp_t>& ray) const restrict(amp)
	{
		vector3<fp_t> v = ray.origin - center;
		fp_t a0 = v.sqr_length() - sqr_radius;
		fp_t d_dot_v = ray.direction.dot(v);

		if (d_dot_v <= 0 )
		{
			fp_t discr = d_dot_v * d_dot_v - a0;
			fp_t distance = -d_dot_v - math_helper<fp_t>::sqrt(discr);
			vector3<fp_t> position(ray.get_point(distance));

			if (discr >= 0)
			{
				return intersect_result<fp_t>(
					true, 
					0, 
					distance,
					position,
					(position - center).normalize()
					);

			}
		}

		return intersect_result<fp_t>();
	}

private:
	fp_t sqr_radius;
	void init()
	{
		sqr_radius = radius * radius;
	}
};