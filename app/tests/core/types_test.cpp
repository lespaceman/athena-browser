#include <gtest/gtest.h>
#include "core/types.h"

using namespace athena::core;

// Point Tests
TEST(PointTest, DefaultConstructor) {
  Point p;
  EXPECT_EQ(p.x, 0);
  EXPECT_EQ(p.y, 0);
}

TEST(PointTest, ParameterizedConstructor) {
  Point p(10, 20);
  EXPECT_EQ(p.x, 10);
  EXPECT_EQ(p.y, 20);
}

TEST(PointTest, Equality) {
  Point p1(10, 20);
  Point p2(10, 20);
  Point p3(5, 10);

  EXPECT_EQ(p1, p2);
  EXPECT_NE(p1, p3);
}

TEST(PointTest, ToString) {
  Point p(10, 20);
  EXPECT_EQ(p.ToString(), "Point(10, 20)");
}

// Size Tests
TEST(SizeTest, DefaultConstructor) {
  Size s;
  EXPECT_EQ(s.width, 0);
  EXPECT_EQ(s.height, 0);
}

TEST(SizeTest, ParameterizedConstructor) {
  Size s(800, 600);
  EXPECT_EQ(s.width, 800);
  EXPECT_EQ(s.height, 600);
}

TEST(SizeTest, Equality) {
  Size s1(800, 600);
  Size s2(800, 600);
  Size s3(1024, 768);

  EXPECT_EQ(s1, s2);
  EXPECT_NE(s1, s3);
}

TEST(SizeTest, IsEmpty) {
  EXPECT_TRUE(Size(0, 0).IsEmpty());
  EXPECT_TRUE(Size(-1, 100).IsEmpty());
  EXPECT_TRUE(Size(100, 0).IsEmpty());
  EXPECT_FALSE(Size(100, 100).IsEmpty());
}

TEST(SizeTest, Area) {
  Size s(800, 600);
  EXPECT_EQ(s.Area(), 480000);
}

TEST(SizeTest, ToString) {
  Size s(800, 600);
  EXPECT_EQ(s.ToString(), "Size(800x600)");
}

// Rect Tests
TEST(RectTest, DefaultConstructor) {
  Rect r;
  EXPECT_EQ(r.x, 0);
  EXPECT_EQ(r.y, 0);
  EXPECT_EQ(r.width, 0);
  EXPECT_EQ(r.height, 0);
}

TEST(RectTest, ParameterizedConstructor) {
  Rect r(10, 20, 800, 600);
  EXPECT_EQ(r.x, 10);
  EXPECT_EQ(r.y, 20);
  EXPECT_EQ(r.width, 800);
  EXPECT_EQ(r.height, 600);
}

TEST(RectTest, PointSizeConstructor) {
  Point p(10, 20);
  Size s(800, 600);
  Rect r(p, s);

  EXPECT_EQ(r.x, 10);
  EXPECT_EQ(r.y, 20);
  EXPECT_EQ(r.width, 800);
  EXPECT_EQ(r.height, 600);
}

TEST(RectTest, Equality) {
  Rect r1(10, 20, 800, 600);
  Rect r2(10, 20, 800, 600);
  Rect r3(0, 0, 100, 100);

  EXPECT_EQ(r1, r2);
  EXPECT_NE(r1, r3);
}

TEST(RectTest, Origin) {
  Rect r(10, 20, 800, 600);
  Point origin = r.Origin();

  EXPECT_EQ(origin.x, 10);
  EXPECT_EQ(origin.y, 20);
}

TEST(RectTest, GetSize) {
  Rect r(10, 20, 800, 600);
  Size size = r.GetSize();

  EXPECT_EQ(size.width, 800);
  EXPECT_EQ(size.height, 600);
}

TEST(RectTest, RightBottom) {
  Rect r(10, 20, 800, 600);
  EXPECT_EQ(r.Right(), 810);
  EXPECT_EQ(r.Bottom(), 620);
}

TEST(RectTest, IsEmpty) {
  EXPECT_TRUE(Rect(0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(Rect(10, 20, 0, 100).IsEmpty());
  EXPECT_TRUE(Rect(10, 20, 100, 0).IsEmpty());
  EXPECT_FALSE(Rect(10, 20, 100, 100).IsEmpty());
}

TEST(RectTest, Area) {
  Rect r(10, 20, 800, 600);
  EXPECT_EQ(r.Area(), 480000);
}

TEST(RectTest, ContainsPoint) {
  Rect r(10, 20, 100, 100);

  EXPECT_TRUE(r.Contains(Point(10, 20)));
  EXPECT_TRUE(r.Contains(Point(50, 50)));
  EXPECT_TRUE(r.Contains(Point(109, 119)));
  EXPECT_FALSE(r.Contains(Point(110, 120)));
  EXPECT_FALSE(r.Contains(Point(0, 0)));
  EXPECT_FALSE(r.Contains(Point(120, 130)));
}

TEST(RectTest, ContainsRect) {
  Rect r1(10, 20, 100, 100);
  Rect r2(20, 30, 50, 50);
  Rect r3(0, 0, 50, 50);

  EXPECT_TRUE(r1.Contains(r2));
  EXPECT_FALSE(r1.Contains(r3));
  EXPECT_TRUE(r1.Contains(r1));
}

TEST(RectTest, Intersects) {
  Rect r1(10, 20, 100, 100);
  Rect r2(50, 60, 100, 100);
  Rect r3(200, 200, 100, 100);

  EXPECT_TRUE(r1.Intersects(r2));
  EXPECT_FALSE(r1.Intersects(r3));
  EXPECT_TRUE(r1.Intersects(r1));
}

TEST(RectTest, Intersection) {
  Rect r1(10, 20, 100, 100);
  Rect r2(50, 60, 100, 100);
  Rect result = r1.Intersection(r2);

  EXPECT_EQ(result.x, 50);
  EXPECT_EQ(result.y, 60);
  EXPECT_EQ(result.width, 60);
  EXPECT_EQ(result.height, 60);
}

TEST(RectTest, IntersectionNoOverlap) {
  Rect r1(10, 20, 100, 100);
  Rect r2(200, 200, 100, 100);
  Rect result = r1.Intersection(r2);

  EXPECT_TRUE(result.IsEmpty());
}

TEST(RectTest, Union) {
  Rect r1(10, 20, 100, 100);
  Rect r2(50, 60, 150, 150);
  Rect result = r1.Union(r2);

  EXPECT_EQ(result.x, 10);
  EXPECT_EQ(result.y, 20);
  EXPECT_EQ(result.width, 190);
  EXPECT_EQ(result.height, 190);
}

TEST(RectTest, UnionWithEmpty) {
  Rect r1(10, 20, 100, 100);
  Rect r2;
  Rect result = r1.Union(r2);

  EXPECT_EQ(result, r1);
}

TEST(RectTest, ToString) {
  Rect r(10, 20, 800, 600);
  EXPECT_EQ(r.ToString(), "Rect(10, 20, 800x600)");
}

// ScaleFactor Tests
TEST(ScaleFactorTest, DefaultConstructor) {
  ScaleFactor sf;
  EXPECT_EQ(sf.value, 1.0f);
}

TEST(ScaleFactorTest, ParameterizedConstructor) {
  ScaleFactor sf(2.0f);
  EXPECT_EQ(sf.value, 2.0f);
}

TEST(ScaleFactorTest, Equality) {
  ScaleFactor sf1(2.0f);
  ScaleFactor sf2(2.0f);
  ScaleFactor sf3(1.5f);

  EXPECT_EQ(sf1, sf2);
  EXPECT_NE(sf1, sf3);
}

TEST(ScaleFactorTest, Comparison) {
  ScaleFactor sf1(1.0f);
  ScaleFactor sf2(2.0f);

  EXPECT_LT(sf1, sf2);
  EXPECT_GT(sf2, sf1);
}

TEST(ScaleFactorTest, Multiplication) {
  ScaleFactor sf1(2.0f);
  ScaleFactor sf2(1.5f);
  ScaleFactor result = sf1 * sf2;

  EXPECT_EQ(result.value, 3.0f);
}

TEST(ScaleFactorTest, Division) {
  ScaleFactor sf1(3.0f);
  ScaleFactor sf2(2.0f);
  ScaleFactor result = sf1 / sf2;

  EXPECT_EQ(result.value, 1.5f);
}

TEST(ScaleFactorTest, ScaleInt) {
  ScaleFactor sf(2.0f);
  EXPECT_EQ(sf.Scale(100), 200);
  EXPECT_EQ(sf.Scale(50), 100);
}

TEST(ScaleFactorTest, UnscaleInt) {
  ScaleFactor sf(2.0f);
  EXPECT_EQ(sf.Unscale(200), 100);
  EXPECT_EQ(sf.Unscale(100), 50);
}

TEST(ScaleFactorTest, ScalePoint) {
  ScaleFactor sf(2.0f);
  Point p(100, 200);
  Point result = sf.Scale(p);

  EXPECT_EQ(result.x, 200);
  EXPECT_EQ(result.y, 400);
}

TEST(ScaleFactorTest, UnscalePoint) {
  ScaleFactor sf(2.0f);
  Point p(200, 400);
  Point result = sf.Unscale(p);

  EXPECT_EQ(result.x, 100);
  EXPECT_EQ(result.y, 200);
}

TEST(ScaleFactorTest, ScaleSize) {
  ScaleFactor sf(2.0f);
  Size s(100, 200);
  Size result = sf.Scale(s);

  EXPECT_EQ(result.width, 200);
  EXPECT_EQ(result.height, 400);
}

TEST(ScaleFactorTest, UnscaleSize) {
  ScaleFactor sf(2.0f);
  Size s(200, 400);
  Size result = sf.Unscale(s);

  EXPECT_EQ(result.width, 100);
  EXPECT_EQ(result.height, 200);
}

TEST(ScaleFactorTest, ScaleRect) {
  ScaleFactor sf(2.0f);
  Rect r(10, 20, 100, 200);
  Rect result = sf.Scale(r);

  EXPECT_EQ(result.x, 20);
  EXPECT_EQ(result.y, 40);
  EXPECT_EQ(result.width, 200);
  EXPECT_EQ(result.height, 400);
}

TEST(ScaleFactorTest, UnscaleRect) {
  ScaleFactor sf(2.0f);
  Rect r(20, 40, 200, 400);
  Rect result = sf.Unscale(r);

  EXPECT_EQ(result.x, 10);
  EXPECT_EQ(result.y, 20);
  EXPECT_EQ(result.width, 100);
  EXPECT_EQ(result.height, 200);
}

TEST(ScaleFactorTest, ToString) {
  ScaleFactor sf(2.0f);
  EXPECT_EQ(sf.ToString(), "ScaleFactor(2)");
}
