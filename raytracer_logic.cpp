#pragma once

#include <image.h>
#include <camera_options.h>
#include <render_options.h>
#include <string>
#include <vector.h>
#include <initializer_list>
#include <ray.h>
#include <scene.h>
#include <intersection.h>
#include <geometry.h>
#include <cmath>
#include <transformer.h>
#include <postprocessing.h>

void RayCast(const Scene& scene, const Ray& view_ray, RenderOptions opt, Vector& result);

template <class T>
auto FindNearestIntersection(const std::vector<T>& objects, const Ray& ray,
                             Intersection& best_intersection, bool flag) {
    const T* target_object = nullptr;
    for (auto& obj : objects) {
        auto intersection = GetIntersection(ray, obj.GetObject());
        if (intersection && ((!flag && !target_object) ||
                             intersection->GetDistance() < best_intersection.GetDistance())) {
            best_intersection = intersection.value();
            target_object = &obj;
        }
    }
    return target_object;
}

inline bool IsInShadow(const Scene& scene, const Light& light, const Vector& pos) {
    auto light_vector = pos - light.position;
    light_vector.Normalize();
    Ray light_ray(light.position, light_vector);
    // find intersection
    Intersection best_intersection;
    bool triangle_f = false;
    auto triangle =
        FindNearestIntersection(scene.GetObjects(), light_ray, best_intersection, triangle_f);
    triangle_f = triangle != nullptr;
    FindNearestIntersection(scene.GetSphereObjects(), light_ray, best_intersection, triangle_f);
    return (Length(pos - best_intersection.GetPosition()) > 1e-6);
}

template <class T>
void GetColor(const Scene& scene, const Ray& view_ray, RenderOptions opt,
              const Intersection& vis_intersection, const T& vis_obj, Vector& result) {
    if (opt.mode == RenderMode::kDepth) {
        auto d = vis_intersection.GetDistance();
        result = {d, d, d};
    } else if (opt.mode == RenderMode::kNormal) {
        auto& norm = vis_intersection.GetNormal();
        result = {norm[0], norm[1], norm[2]};
    } else {
        Vector general, diffusion, specular, reflected, refracted;
        general = vis_obj.material->intensity + vis_obj.material->ambient_color;
        // Specular and diffusion
        for (const auto& light : scene.GetLights()) {
            auto light_vector = vis_intersection.GetPosition() - light.position;
            light_vector.Normalize();
            Ray light_ray(light.position, light_vector);
            // check is vis_obj in shadow
            if (IsInShadow(scene, light, vis_intersection.GetPosition())) {
                continue;
            }
            double l_d =
                std::max(0.0, DotProduct(-light_ray.GetDirection(), vis_intersection.GetNormal()));
            double l_s = std::pow(std::max(0.0, DotProduct(-view_ray.GetDirection(),
                                                           Reflect(light_ray.GetDirection(),
                                                                   vis_intersection.GetNormal()))),
                                  vis_obj.material->specular_exponent);
            diffusion = diffusion + l_d * light.intensity * vis_obj.material->diffuse_color;
            specular = specular + l_s * light.intensity * vis_obj.material->specular_color;
        }
        // Refraction
        opt.depth -= 1;
        bool inside = vis_obj.IsInside(view_ray.GetDirection(), vis_intersection.GetNormal());
        if (vis_obj.material->albedo[2] > 0) {
            double eta = 1.0 / vis_obj.material->refraction_index;
            auto refracted_ray =
                Refract(view_ray.GetDirection(), vis_intersection.GetNormal(), eta);
            refracted_ray->Normalize();
            if (refracted_ray) {
                Vector origin = vis_intersection.GetPosition() + 1e-5 * refracted_ray.value();
                RayCast(scene, Ray(origin, refracted_ray.value()), opt, refracted);
                if (!inside) {
                    refracted = refracted * vis_obj.material->albedo[2];
                }
            }
        }
        // Reflection
        if (vis_obj.material->albedo[1] > 0 && !inside) {
            Vector origin = vis_intersection.GetPosition() + 1e-5 * vis_intersection.GetNormal();
            Vector reflected_ray = Reflect(view_ray.GetDirection(), vis_intersection.GetNormal());
            reflected_ray.Normalize();
            RayCast(scene, Ray(origin, reflected_ray), opt, reflected);
        }
        result = general + vis_obj.material->albedo[0] * (diffusion + specular) +
                 vis_obj.material->albedo[1] * reflected + refracted;
    }
}

void RayCast(const Scene& scene, const Ray& view_ray, RenderOptions opt, Vector& result) {
    if (opt.depth == 0) {
        result = Vector();
    } else {
        // Find visible object
        Intersection best_intersection;
        // flag is true if triangle was found
        bool triangle_f = false;
        auto triangle =
            FindNearestIntersection(scene.GetObjects(), view_ray, best_intersection, triangle_f);
        triangle_f = triangle != nullptr;
        auto sphere = FindNearestIntersection(scene.GetSphereObjects(), view_ray, best_intersection,
                                              triangle_f);
        if (sphere != nullptr) {
            // Sphere is visible!
            GetColor(scene, view_ray, opt, best_intersection, *sphere, result);
        } else if (triangle != nullptr) {
            // Triangle is visible
            DefineNormal(*triangle, best_intersection);
            GetColor(scene, view_ray, opt, best_intersection, *triangle, result);
        } else {
            // Nothing => backgroud is visible
            result = Vector();
        }
    }
}

Image Render(const std::string& filename, const CameraOptions& camera_options,
             const RenderOptions& render_options) {
    auto scene = ReadScene(filename);
    Image img(camera_options.screen_width, camera_options.screen_height);
    Transformer transformer(camera_options);

    std::vector<std::vector<Vector>> color_map(img.Height(), std::vector<Vector>(img.Width()));
    for (int i = 0; i < img.Height(); ++i) {
        for (int j = 0; j < img.Width(); ++j) {
            Ray view_ray = transformer.MakeRay(i, j);
            RayCast(scene, view_ray, render_options, color_map[i][j]);
        }
    }

    PostProc(img, color_map, render_options);
    return img;
}
