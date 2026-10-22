#include "albedo/scattering/lobe.hpp"

#include "albedo/core/vector3.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

// Has `evaluate` and `sample` but no `pdf`, so it must not count as a lobe.
struct LobeWithoutPdf {
    [[nodiscard]] static Rgb evaluate(Vector3r /*wo*/, Vector3r /*wi*/) noexcept { return {}; }
    [[nodiscard]] static std::optional<ScatterSample> sample(Vector3r /*wo*/, Real /*u_choice*/,
                                                             Point2r /*u*/) noexcept
    {
        return std::nullopt;
    }
};

template <typename... Lobes>
concept CanFormLobeVariant = requires { typename LobeVariant<Lobes...>; };

const Vector3r up{0.0, 0.0, 1.0};
const Vector3r tilted_out{normalized(Vector3r{0.3, -0.4, 0.8})};

TEST(LobeTest, typeWithoutPdfIsNotALobe)
{
    static_assert(LobeLike<LambertLobe>);
    static_assert(LobeLike<ConductorLobe>);
    static_assert(LobeLike<DielectricLobe>);
    static_assert(!LobeLike<LobeWithoutPdf>);
}

TEST(LobeTest, lobeVariantRejectsNonLobeAlternatives)
{
    static_assert(CanFormLobeVariant<LambertLobe>);
    static_assert(!CanFormLobeVariant<LambertLobe, LobeWithoutPdf>);
}

constexpr LambertLobe lambert{.albedo = {0.1, 0.4, 0.9}};

TEST(LobeTest, forwardsEvaluate)
{
    const Lobe lobe{lambert};

    expectNear(evaluate(lobe, up, tilted_out), lambert.evaluate(up, tilted_out));
}

TEST(LobeTest, forwardsPdf)
{
    const Lobe lobe{lambert};

    EXPECT_NEAR(pdf(lobe, up, tilted_out), lambert.pdf(up, tilted_out), default_tolerance);
}

TEST(LobeTest, forwardsSample)
{
    const Lobe lobe{lambert};

    expectNear(requireValue(sample(lobe, up, 0.5, {0.25, 0.75})).direction,
               requireValue(lambert.sample(up, 0.5, {0.25, 0.75})).direction);
}

TEST(LobeTest, lambertIgnoresTheChoiceNumber)
{
    const ScatterSample low{requireValue(lambert.sample(tilted_out, 0.0, {0.25, 0.75}))};
    const ScatterSample high{requireValue(lambert.sample(tilted_out, 0.99, {0.25, 0.75}))};

    expectNear(low.direction, high.direction, test_support::exact_tolerance);
    expectNear(low.weight, high.weight, test_support::exact_tolerance);
}

TEST(LobeTest, conductorIgnoresTheChoiceNumber)
{
    const ConductorLobe conductor{.eta = {0.2, 0.9, 1.4}, .k = {3.0, 2.4, 1.6}, .alpha = 0.4};

    const ScatterSample low{requireValue(conductor.sample(tilted_out, 0.0, {0.25, 0.75}))};
    const ScatterSample high{requireValue(conductor.sample(tilted_out, 0.99, {0.25, 0.75}))};

    expectNear(low.direction, high.direction, test_support::exact_tolerance);
    expectNear(low.weight, high.weight, test_support::exact_tolerance);
}

} // namespace
} // namespace albedo
