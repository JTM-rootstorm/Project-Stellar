#include "stellar/scene/AnimationRuntime.hpp"

#include <cassert>
#include <cmath>

namespace {

bool near(float lhs, float rhs) {
    return std::abs(lhs - rhs) < 0.0001f;
}

stellar::assets::AnimationSamplerAsset sampler(std::vector<float> times, std::vector<float> values,
                                               std::size_t components,
                                               stellar::assets::AnimationInterpolation mode) {
    return stellar::assets::AnimationSamplerAsset{.input_times = std::move(times),
                                                  .output_values = std::move(values),
                                                  .output_components = components,
                                                  .interpolation = mode};
}

stellar::assets::SceneAsset make_animation_scene() {
    stellar::assets::SceneAsset scene;
    stellar::scene::Node root;
    root.name = "root";
    root.children = {1};
    root.local_transform.translation = {1.0f, 0.0f, 0.0f};
    stellar::scene::Node child;
    child.name = "child";
    child.parent_index = 0;
    child.local_transform.translation = {0.0f, 2.0f, 0.0f};
    scene.nodes = {root, child};
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;

    stellar::assets::AnimationAsset animation;
    animation.name = "move";
    animation.samplers.push_back(sampler({0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f},
                                         3, stellar::assets::AnimationInterpolation::kLinear));
    animation.samplers.push_back(sampler({0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 2.0f, 3.0f, 4.0f},
                                         3, stellar::assets::AnimationInterpolation::kStep));
    animation.samplers.push_back(sampler({0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f,
                                                        0.0f, 0.0f, 1.0f, 0.0f},
                                         4, stellar::assets::AnimationInterpolation::kLinear));
    animation.channels.push_back(stellar::assets::AnimationChannelAsset{
        .sampler_index = 0,
        .target_node = 0,
        .target_path = stellar::assets::AnimationTargetPath::kTranslation,
    });
    animation.channels.push_back(stellar::assets::AnimationChannelAsset{
        .sampler_index = 1,
        .target_node = 1,
        .target_path = stellar::assets::AnimationTargetPath::kScale,
    });
    animation.channels.push_back(stellar::assets::AnimationChannelAsset{
        .sampler_index = 2,
        .target_node = 1,
        .target_path = stellar::assets::AnimationTargetPath::kRotation,
    });
    scene.animations.push_back(animation);
    return scene;
}

void test_interpolation_and_node_updates() {
    auto scene = make_animation_scene();
    stellar::scene::AnimationPlayer player;
    player.bind(scene);
    assert(player.select_animation("move").has_value());
    assert(player.evaluate(0.5f).has_value());

    const auto& pose = player.pose();
    assert(near(pose.local_transforms[0].translation[0], 5.0f));
    assert(near(pose.local_transforms[1].scale[0], 1.0f));
    assert(near(pose.local_transforms[1].rotation[2], 0.7071067f));
    assert(near(pose.local_transforms[1].rotation[3], 0.7071067f));
    assert(near(pose.world_transforms[1][12], 5.0f));
    assert(near(pose.world_transforms[1][13], 2.0f));

    player.set_looping(true);
    player.play();
    assert(player.update(1.25f).has_value());
    assert(near(player.current_time(), 0.75f));
    player.pause();
    assert(!player.is_playing());
    player.stop();
    assert(near(player.pose().local_transforms[0].translation[0], 1.0f));
}

void test_cubic_spline_translation() {
    stellar::assets::SceneAsset scene;
    scene.nodes.push_back(stellar::scene::Node{.name = "node"});
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
    stellar::assets::AnimationAsset animation;
    animation.samplers.push_back(sampler({0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                        10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
                                         3, stellar::assets::AnimationInterpolation::kCubicSpline));
    animation.channels.push_back(stellar::assets::AnimationChannelAsset{
        .sampler_index = 0,
        .target_node = 0,
        .target_path = stellar::assets::AnimationTargetPath::kTranslation,
    });
    scene.animations.push_back(animation);

    stellar::scene::AnimationPlayer player;
    player.bind(scene);
    assert(player.select_animation(0).has_value());
    assert(player.evaluate(0.5f).has_value());
    assert(near(player.pose().local_transforms[0].translation[0], 5.0f));
}

void test_skeleton_pose() {
    stellar::assets::SceneAsset scene;
    stellar::scene::Node root;
    root.children = {1};
    root.local_transform.translation = {2.0f, 0.0f, 0.0f};
    stellar::scene::Node joint;
    joint.parent_index = 0;
    joint.local_transform.translation = {0.0f, 3.0f, 0.0f};
    scene.nodes = {root, joint};
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
    stellar::assets::SkinAsset skin;
    skin.joints = {1};
    skin.inverse_bind_matrices = {{{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 1.0f, 0.0f, -2.0f, -1.0f, 0.0f, 1.0f}}};
    scene.skins.push_back(skin);

    stellar::scene::AnimationPlayer player;
    player.bind(scene);
    const auto& joint_matrix = player.pose().skin_poses[0].joint_matrices[0];
    assert(near(joint_matrix[12], 0.0f));
    assert(near(joint_matrix[13], 2.0f));
}

void test_static_no_animation_behavior() {
    stellar::assets::SceneAsset scene;
    stellar::scene::Node node;
    node.local_transform.translation = {4.0f, 5.0f, 6.0f};
    scene.nodes.push_back(node);
    scene.scenes.push_back(stellar::scene::Scene{.name = "default", .root_nodes = {0}});
    scene.default_scene_index = 0;
    stellar::scene::AnimationPlayer player;
    player.bind(scene);
    assert(player.update(0.5f).has_value());
    assert(near(player.pose().local_transforms[0].translation[0], 4.0f));
    assert(near(player.pose().world_transforms[0][13], 5.0f));
}

} // namespace

int main() {
    test_interpolation_and_node_updates();
    test_cubic_spline_translation();
    test_skeleton_pose();
    test_static_no_animation_behavior();
    return 0;
}
