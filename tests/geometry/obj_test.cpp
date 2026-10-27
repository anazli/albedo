#include "albedo/geometry/obj.hpp"

#include "support/expect_near.hpp"
#include "support/temporary_file.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::TemporaryFile;

TriangleMesh readObjText(std::string_view text, const ObjLimits& limits = {})
{
    std::istringstream input{std::string{text}};
    return readObj(input, limits);
}

// The line an OBJ error reports, or zero when parsing unexpectedly succeeds.
std::size_t errorLine(std::string_view text, const ObjLimits& limits = {})
{
    try {
        static_cast<void>(readObjText(text, limits));
    } catch (const ObjParseError& error) {
        return error.line();
    }
    return 0;
}

constexpr std::string_view one_triangle{"v 0 0 0\n"
                                        "v 1 0 0\n"
                                        "v 0 1 0\n"
                                        "f 1 2 3\n"};

TEST(ObjTest, readsATriangle)
{
    const TriangleMesh mesh{readObjText(one_triangle)};

    ASSERT_EQ(mesh.positions().size(), 3U);
    expectNear(mesh.positions()[1], Point3r{1.0, 0.0, 0.0}, exact_tolerance);
    ASSERT_EQ(mesh.faces().size(), 1U);
    EXPECT_EQ(mesh.faces()[0], (TriangleMesh::Face{0, 1, 2}));
}

TEST(ObjTest, emptyInputGivesAnEmptyMesh)
{
    const TriangleMesh mesh{readObjText("")};

    EXPECT_TRUE(mesh.positions().empty());
    EXPECT_TRUE(mesh.faces().empty());
}

TEST(ObjTest, ignoresCommentsBlankLinesAndOtherStatements)
{
    const TriangleMesh mesh{readObjText("# exported by a tool\n"
                                        "\n"
                                        "mtllib scene.mtl\n"
                                        "o Thing\n"
                                        "g group\n"
                                        "s 1\n"
                                        "usemtl red\n"
                                        "v 0 0 0\n"
                                        "v 1 0 0\n"
                                        "v 0 1 0\n"
                                        "vn 0 0 1\n"
                                        "vt 0.5 0.5\n"
                                        "l 1 2\n"
                                        "f 1 2 3\n")};

    EXPECT_EQ(mesh.positions().size(), 3U);
    EXPECT_EQ(mesh.faces().size(), 1U);
}

TEST(ObjTest, ignoresTrailingComments)
{
    const TriangleMesh mesh{readObjText("v 0 0 0 # origin\n"
                                        "v 1 0 0\n"
                                        "v 0 1 0\n"
                                        "f 1 2 3 # the only face\n")};

    EXPECT_EQ(mesh.faces().size(), 1U);
}

TEST(ObjTest, acceptsEveryFaceVertexForm)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                        "vt 0 0\nvn 0 0 1\n"
                                        "f 1/1 2/1 3/1\n"
                                        "f 1//1 2//1 3//1\n"
                                        "f 1/1/1 2/1/1 3/1/1\n")};

    ASSERT_EQ(mesh.faces().size(), 3U);
    for (const TriangleMesh::Face& face : mesh.faces()) {
        EXPECT_EQ(face, (TriangleMesh::Face{0, 1, 2}));
    }
}

TEST(ObjTest, negativeIndicesCountBackFromTheLatestVertex)
{
    const TriangleMesh mesh{readObjText("v 9 9 9\n"
                                        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                        "f -3 -2 -1\n")};

    ASSERT_EQ(mesh.faces().size(), 1U);
    EXPECT_EQ(mesh.faces()[0], (TriangleMesh::Face{1, 2, 3}));
}

TEST(ObjTest, triangulatesPolygonsAsAFan)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv -1 1 0\n"
                                        "f 1 2 3 4 5\n")};

    ASSERT_EQ(mesh.faces().size(), 3U);
    EXPECT_EQ(mesh.faces()[0], (TriangleMesh::Face{0, 1, 2}));
    EXPECT_EQ(mesh.faces()[1], (TriangleMesh::Face{0, 2, 3}));
    EXPECT_EQ(mesh.faces()[2], (TriangleMesh::Face{0, 3, 4}));
}

TEST(ObjTest, acceptsWindowsLineEndingsTabsAndNoFinalNewline)
{
    const TriangleMesh mesh{readObjText("v\t0 0 0\r\nv 1\t0 0\r\n  v 0 1 0\r\nf 1 2 3")};

    EXPECT_EQ(mesh.positions().size(), 3U);
    EXPECT_EQ(mesh.faces().size(), 1U);
}

TEST(ObjTest, acceptsAWeightAndVertexColorsAfterThePosition)
{
    const TriangleMesh mesh{readObjText("v 0 0 0 1\nv 1 0 0 0.5 0.5 0.5\nv 0 1 0\nf 1 2 3\n")};

    ASSERT_EQ(mesh.positions().size(), 3U);
    expectNear(mesh.positions()[1], Point3r{1.0, 0.0, 0.0}, exact_tolerance);
}

TEST(ObjTest, parsesSignsAndExponents)
{
    const TriangleMesh mesh{readObjText("v +1.5e0 -2E-1 .25\n")};

    ASSERT_EQ(mesh.positions().size(), 1U);
    expectNear(mesh.positions()[0], Point3r{1.5, -0.2, 0.25}, exact_tolerance);
}

TEST(ObjTest, keepsFacesWithoutAreaForTheMeshToSkip)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nf 1 1 2\n")};

    EXPECT_EQ(mesh.faces().size(), 1U);
    EXPECT_TRUE(mesh.triangles().empty());
}

TEST(ObjTest, errorsNameTheLine)
{
    try {
        static_cast<void>(readObjText("v 0 0 0\nv 1 0\n"));
        FAIL() << "expected an ObjParseError";
    } catch (const ObjParseError& error) {
        EXPECT_EQ(error.line(), 2U);
        EXPECT_NE(std::string_view{error.what()}.find("line 2"), std::string_view::npos);
    }
}

TEST(ObjTest, rejectsVerticesWithTooFewCoordinates)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0\n"), 2U);
    EXPECT_EQ(errorLine("v\n"), 1U);
}

TEST(ObjTest, rejectsMalformedNumbers)
{
    EXPECT_EQ(errorLine("v 1.0abc 0 0\n"), 1U);
    EXPECT_EQ(errorLine("v 1..2 0 0\n"), 1U);
    EXPECT_EQ(errorLine("v 0 0 0\nv - 0 0\n"), 2U);
    EXPECT_EQ(errorLine("v ++1 0 0\n"), 1U);
    EXPECT_EQ(errorLine("v 0 0 0 x\n"), 1U);
}

TEST(ObjTest, rejectsNonFiniteAndOutOfRangeCoordinates)
{
    EXPECT_EQ(errorLine("v nan 0 0\n"), 1U);
    EXPECT_EQ(errorLine("v 0 inf 0\n"), 1U);
    EXPECT_EQ(errorLine("v 0 0 -infinity\n"), 1U);
    EXPECT_EQ(errorLine("v 1e400 0 0\n"), 1U);
}

TEST(ObjTest, rejectsFacesWithFewerThanThreeVertices)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nf 1 2\n"), 3U);
    EXPECT_EQ(errorLine("f\n"), 1U);
}

TEST(ObjTest, rejectsIndexZero)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n"), 4U);
}

TEST(ObjTest, rejectsIndicesBeyondTheVerticesReadSoFar)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n"), 4U);
    // A vertex defined after the face does not count.
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nf 1 2 3\nv 0 1 0\n"), 3U);
}

TEST(ObjTest, rejectsNegativeIndicesReachingBeforeTheFirstVertex)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -1 -2 -4\n"), 4U);
}

TEST(ObjTest, rejectsIndicesTooLargeForAnyInteger)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 99999999999999999999999\n"), 4U);
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 -99999999999999999999999\n"), 4U);
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 -9223372036854775808\n"), 4U);
}

TEST(ObjTest, rejectsMalformedFaceVertices)
{
    constexpr std::string_view vertices{"v 0 0 0\nv 1 0 0\nv 0 1 0\n"};
    for (const std::string_view corner : {"a", "1/2/3/4", "1/x", "/1", "1/1/y", "1.5", "+1", ""}) {
        const std::string text{std::string{vertices} + "f 1 2 " + std::string{corner} +
                               (corner.empty() ? "/\n" : "\n")};

        EXPECT_EQ(errorLine(text), 4U) << "corner: " << corner;
    }
}

// Lines of exactly 16 bytes fit this limit; the tests below probe either side.
constexpr ObjLimits short_lines{.max_line_length = 16};

TEST(ObjTest, acceptsLinesAtTheLengthLimitWithAnyEnding)
{
    EXPECT_NO_THROW(static_cast<void>(readObjText("v 0.00000001 0 0\n", short_lines)));
    EXPECT_NO_THROW(static_cast<void>(readObjText("v 0.00000001 0 0\r\n", short_lines)));
    EXPECT_NO_THROW(static_cast<void>(readObjText("v 0.00000001 0 0", short_lines)));
    EXPECT_NO_THROW(static_cast<void>(readObjText("v 0.00000001 0 0\r", short_lines)));
}

TEST(ObjTest, rejectsLinesOverTheLengthLimit)
{
    // 20 bytes overflows the read buffer; 17 bytes fits it but exceeds the limit,
    // with or without a final newline.
    EXPECT_EQ(errorLine("v 0 0 0\nv 0.000000000001 0 0\n", short_lines), 2U);
    EXPECT_EQ(errorLine("v 0 0 0\nv 0.000000001 0 0\n", short_lines), 2U);
    EXPECT_EQ(errorLine("v 0 0 0\nv 0.000000001 0 0", short_lines), 2U);
    EXPECT_EQ(errorLine("v 0 0 0\nv 0.000000001 0 0\r\n", short_lines), 2U);
}

TEST(ObjTest, rejectsALineLengthLimitTooLargeToBuffer)
{
    std::istringstream input{std::string{one_triangle}};
    const ObjLimits limits{.max_line_length = std::numeric_limits<std::size_t>::max()};

    EXPECT_THROW(static_cast<void>(readObj(input, limits)), std::invalid_argument);
}

TEST(ObjTest, acceptsVerticesUpToTheLimit)
{
    EXPECT_NO_THROW(static_cast<void>(readObjText(one_triangle, {.max_vertices = 3})));
}

TEST(ObjTest, rejectsMoreVerticesThanTheLimit)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n", {.max_vertices = 3}), 4U);
}

// Four vertices, so faces can be triangles or a quad (two triangles).
constexpr std::string_view square_vertices{"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"};

TEST(ObjTest, acceptsTrianglesUpToTheLimitAcrossFaces)
{
    const std::string text{std::string{square_vertices} + "f 1 2 3\nf 1 2 3 4\n"};

    EXPECT_EQ(readObjText(text, {.max_triangles = 3}).faces().size(), 3U);
}

TEST(ObjTest, rejectsMoreTrianglesThanTheLimitAcrossFaces)
{
    const std::string text{std::string{square_vertices} + "f 1 2 3 4\nf 1 2 3 4\n"};

    EXPECT_EQ(errorLine(text, {.max_triangles = 3}), 6U);
}

TEST(ObjTest, rejectsAPolygonWithMoreTrianglesThanTheLimit)
{
    const std::string text{std::string{square_vertices} + "f 1 2 3 4\n"};

    EXPECT_EQ(errorLine(text, {.max_triangles = 1}), 5U);
}

TEST(ObjTest, acceptsEmptyTextureAndNormalParts)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/ 2// 3//\n")};

    EXPECT_EQ(mesh.faces().size(), 1U);
}

TEST(ObjTest, readsTextureCoordinatesWithVDefaultingToZero)
{
    const TriangleMesh mesh{readObjText("vt 0.25\n"
                                        "vt 0.5 0.75\n"
                                        "vt 0.125 1 0.3\n")};

    ASSERT_EQ(mesh.textureCoordinates().size(), 3U);
    expectNear(mesh.textureCoordinates()[0], Point2r{0.25, 0.0}, exact_tolerance);
    expectNear(mesh.textureCoordinates()[1], Point2r{0.5, 0.75}, exact_tolerance);
    expectNear(mesh.textureCoordinates()[2], Point2r{0.125, 1.0}, exact_tolerance);
}

TEST(ObjTest, ignoresNumbersAfterTheThirdTextureCoordinate)
{
    const TriangleMesh mesh{readObjText("vt 0.5 0.75 0 7 8\n")};

    ASSERT_EQ(mesh.textureCoordinates().size(), 1U);
    expectNear(mesh.textureCoordinates()[0], Point2r{0.5, 0.75}, exact_tolerance);
}

TEST(ObjTest, facesWithTextureIndicesGiveTextureFaces)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                        "vt 0 0\nvt 1 0\nvt 0 1\n"
                                        "f 1/3 2/1 3/2\n"
                                        "f 1/1/1 2/2/1 3/3/1\n")};

    ASSERT_EQ(mesh.textureFaces().size(), 2U);
    EXPECT_EQ(mesh.textureFaces()[0], (TriangleMesh::Face{2, 0, 1}));
    EXPECT_EQ(mesh.textureFaces()[1], (TriangleMesh::Face{0, 1, 2}));
}

TEST(ObjTest, polygonTextureFacesFollowTheFan)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                                        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                                        "f 1/4 2/3 3/2 4/1\n")};

    ASSERT_EQ(mesh.textureFaces().size(), 2U);
    EXPECT_EQ(mesh.textureFaces()[0], (TriangleMesh::Face{3, 2, 1}));
    EXPECT_EQ(mesh.textureFaces()[1], (TriangleMesh::Face{3, 1, 0}));
}

TEST(ObjTest, negativeTextureIndicesCountBackFromTheLatestTextureCoordinate)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                        "vt 9 9\nvt 0 0\nvt 1 0\nvt 0 1\n"
                                        "f 1/-3 2/-2 3/-1\n")};

    ASSERT_EQ(mesh.textureFaces().size(), 1U);
    EXPECT_EQ(mesh.textureFaces()[0], (TriangleMesh::Face{1, 2, 3}));
}

TEST(ObjTest, facesWithoutTextureIndicesGiveNoTextureFaces)
{
    const TriangleMesh mesh{readObjText(std::string{"vt 0 0\n"} + std::string{one_triangle})};

    EXPECT_EQ(mesh.faces().size(), 1U);
    EXPECT_TRUE(mesh.textureFaces().empty());
    EXPECT_EQ(mesh.textureCoordinates().size(), 1U);
}

TEST(ObjTest, oneFaceWithoutTextureIndicesDropsTextureFacesForTheMesh)
{
    // The mesh has one texture face per face or none, so a face lacking texture
    // indices on any corner leaves the whole mesh without them, in either order.
    constexpr std::string_view vertices{"v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\n"};
    for (const std::string_view faces :
         {"f 1/1 2/1 3/1\nf 1 2 3\n", "f 1/1 2/1 3/1\nf 1/1 2 3/1\n",
          "f 1/1 2/1 3/1\nf 1// 2//1 3//\n", "f 1 2 3\nf 1/1 2/1 3/1\n"}) {
        const TriangleMesh mesh{readObjText(std::string{vertices} + std::string{faces})};

        EXPECT_EQ(mesh.faces().size(), 2U) << "faces: " << faces;
        EXPECT_TRUE(mesh.textureFaces().empty()) << "faces: " << faces;
        EXPECT_EQ(mesh.textureCoordinates().size(), 1U) << "faces: " << faces;
    }
}

TEST(ObjTest, polygonWithAMiddleCornerWithoutTextureIndexHasNoTextureFaces)
{
    const TriangleMesh mesh{readObjText("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\n"
                                        "f 1/1 2/1 3 4/1\n")};

    EXPECT_EQ(mesh.faces().size(), 2U);
    EXPECT_TRUE(mesh.textureFaces().empty());
}

TEST(ObjTest, textureIndexErrorsNameTextureCoordinates)
{
    try {
        static_cast<void>(readObjText("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/2\n"));
        FAIL() << "expected an ObjParseError";
    } catch (const ObjParseError& error) {
        EXPECT_NE(std::string_view{error.what()}.find("texture coordinate"),
                  std::string_view::npos);
    }
}

TEST(ObjTest, rejectsTextureCoordinatesWithoutU)
{
    EXPECT_EQ(errorLine("vt 0 0\nvt\n"), 2U);
}

TEST(ObjTest, rejectsMalformedAndNonFiniteTextureCoordinates)
{
    for (const std::string_view line : {"vt a\n", "vt 0 b\n", "vt 0 0 c\n", "vt nan\n",
                                        "vt 0 inf\n", "vt 1e999\n", "vt 0 0 0 x\n"}) {
        EXPECT_EQ(errorLine(line), 1U) << "line: " << line;
    }
}

TEST(ObjTest, rejectsTextureIndexZero)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/0 2/1 3/1\n"), 5U);
}

TEST(ObjTest, rejectsTextureIndicesBeyondThoseReadSoFar)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/2\n"), 5U);
    // A texture coordinate defined after the face does not count.
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/1 2/1 3/1\nvt 0 0\n"), 4U);
}

TEST(ObjTest, rejectsNegativeTextureIndicesReachingBeforeTheFirst)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/-1 2/-1 3/-2\n"), 5U);
}

TEST(ObjTest, rejectsTextureIndicesTooLargeForAnyInteger)
{
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/99999999999999999999999\n"),
              5U);
    EXPECT_EQ(
        errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/-99999999999999999999999\n"), 5U);
    EXPECT_EQ(errorLine("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nf 1/1 2/1 3/-9223372036854775808\n"),
              5U);
}

TEST(ObjTest, acceptsTextureCoordinatesUpToTheLimit)
{
    EXPECT_NO_THROW(
        static_cast<void>(readObjText("vt 0 0\nvt 1 0\n", {.max_texture_coordinates = 2})));
}

TEST(ObjTest, rejectsMoreTextureCoordinatesThanTheLimit)
{
    EXPECT_EQ(errorLine("vt 0 0\nvt 1 0\nvt 0 1\n", {.max_texture_coordinates = 2}), 3U);
}

TEST(ObjTest, rejectsAStreamThatIsAlreadyFailed)
{
    std::istringstream input{std::string{one_triangle}};
    input.setstate(std::ios::failbit);

    try {
        static_cast<void>(readObj(input));
        FAIL() << "expected std::runtime_error";
    } catch (const ObjParseError&) {
        FAIL() << "a failed stream is not a parse error";
    } catch (const std::runtime_error&) {
        SUCCEED();
    }
}

TEST(ObjTest, readsAFile)
{
    const TemporaryFile file{"albedo_obj_test_reads_a_file.obj"};
    {
        std::ofstream out{file.path(), std::ios::binary};
        out << one_triangle;
    }

    const TriangleMesh mesh{readObj(file.path())};

    EXPECT_EQ(mesh.faces().size(), 1U);
}

TEST(ObjTest, missingFileThrowsNamingThePath)
{
    const std::filesystem::path missing{test_support::testOutputDirectory() /
                                        "albedo_obj_test_missing_file.obj"};
    std::filesystem::remove(missing);

    try {
        static_cast<void>(readObj(missing));
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string_view{error.what()}.find("albedo_obj_test_missing_file.obj"),
                  std::string_view::npos);
    }
}

TEST(ObjTest, errorsInAFileAreObjParseErrors)
{
    const TemporaryFile file{"albedo_obj_test_bad_file.obj"};
    {
        std::ofstream out{file.path(), std::ios::binary};
        out << "v 0 0 0\nf 1 2 3\n";
    }

    EXPECT_THROW(static_cast<void>(readObj(file.path())), ObjParseError);
}

} // namespace
} // namespace albedo
