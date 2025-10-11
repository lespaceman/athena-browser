#ifndef ATHENA_CORE_TYPES_H_
#define ATHENA_CORE_TYPES_H_

#include <ostream>
#include <string>
#include <sstream>

namespace athena {
namespace core {

// Point represents a 2D coordinate
struct Point {
  int x;
  int y;

  Point() : x(0), y(0) {}
  Point(int x_val, int y_val) : x(x_val), y(y_val) {}

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y;
  }

  bool operator!=(const Point& other) const {
    return !(*this == other);
  }

  std::string ToString() const {
    std::ostringstream oss;
    oss << "Point(" << x << ", " << y << ")";
    return oss.str();
  }
};

inline std::ostream& operator<<(std::ostream& os, const Point& p) {
  return os << p.ToString();
}

// Size represents dimensions (width x height)
struct Size {
  int width;
  int height;

  Size() : width(0), height(0) {}
  Size(int w, int h) : width(w), height(h) {}

  bool operator==(const Size& other) const {
    return width == other.width && height == other.height;
  }

  bool operator!=(const Size& other) const {
    return !(*this == other);
  }

  bool IsEmpty() const {
    return width <= 0 || height <= 0;
  }

  int Area() const {
    return width * height;
  }

  std::string ToString() const {
    std::ostringstream oss;
    oss << "Size(" << width << "x" << height << ")";
    return oss.str();
  }
};

inline std::ostream& operator<<(std::ostream& os, const Size& s) {
  return os << s.ToString();
}

// Rect represents a rectangle (position + size)
struct Rect {
  int x;
  int y;
  int width;
  int height;

  Rect() : x(0), y(0), width(0), height(0) {}
  Rect(int x_val, int y_val, int w, int h)
      : x(x_val), y(y_val), width(w), height(h) {}
  Rect(const Point& origin, const Size& size)
      : x(origin.x), y(origin.y), width(size.width), height(size.height) {}

  bool operator==(const Rect& other) const {
    return x == other.x && y == other.y &&
           width == other.width && height == other.height;
  }

  bool operator!=(const Rect& other) const {
    return !(*this == other);
  }

  Point Origin() const {
    return Point(x, y);
  }

  Size GetSize() const {
    return Size(width, height);
  }

  int Right() const {
    return x + width;
  }

  int Bottom() const {
    return y + height;
  }

  bool IsEmpty() const {
    return width <= 0 || height <= 0;
  }

  int Area() const {
    return width * height;
  }

  bool Contains(const Point& point) const {
    return point.x >= x && point.x < Right() &&
           point.y >= y && point.y < Bottom();
  }

  bool Contains(const Rect& other) const {
    return other.x >= x && other.Right() <= Right() &&
           other.y >= y && other.Bottom() <= Bottom();
  }

  bool Intersects(const Rect& other) const {
    return !(other.x >= Right() || other.Right() <= x ||
             other.y >= Bottom() || other.Bottom() <= y);
  }

  Rect Intersection(const Rect& other) const {
    if (!Intersects(other)) {
      return Rect();
    }

    int left = std::max(x, other.x);
    int top = std::max(y, other.y);
    int right = std::min(Right(), other.Right());
    int bottom = std::min(Bottom(), other.Bottom());

    return Rect(left, top, right - left, bottom - top);
  }

  Rect Union(const Rect& other) const {
    if (IsEmpty()) {
      return other;
    }
    if (other.IsEmpty()) {
      return *this;
    }

    int left = std::min(x, other.x);
    int top = std::min(y, other.y);
    int right = std::max(Right(), other.Right());
    int bottom = std::max(Bottom(), other.Bottom());

    return Rect(left, top, right - left, bottom - top);
  }

  std::string ToString() const {
    std::ostringstream oss;
    oss << "Rect(" << x << ", " << y << ", " << width << "x" << height << ")";
    return oss.str();
  }
};

inline std::ostream& operator<<(std::ostream& os, const Rect& r) {
  return os << r.ToString();
}

// ScaleFactor represents a scaling factor (e.g., HiDPI scaling)
struct ScaleFactor {
  float value;

  ScaleFactor() : value(1.0f) {}
  explicit ScaleFactor(float v) : value(v) {}

  bool operator==(const ScaleFactor& other) const {
    return value == other.value;
  }

  bool operator!=(const ScaleFactor& other) const {
    return value != other.value;
  }

  bool operator<(const ScaleFactor& other) const {
    return value < other.value;
  }

  bool operator>(const ScaleFactor& other) const {
    return value > other.value;
  }

  ScaleFactor operator*(const ScaleFactor& other) const {
    return ScaleFactor(value * other.value);
  }

  ScaleFactor operator/(const ScaleFactor& other) const {
    return ScaleFactor(value / other.value);
  }

  int Scale(int dimension) const {
    return static_cast<int>(dimension * value);
  }

  int Unscale(int dimension) const {
    return static_cast<int>(dimension / value);
  }

  Point Scale(const Point& point) const {
    return Point(Scale(point.x), Scale(point.y));
  }

  Point Unscale(const Point& point) const {
    return Point(Unscale(point.x), Unscale(point.y));
  }

  Size Scale(const Size& size) const {
    return Size(Scale(size.width), Scale(size.height));
  }

  Size Unscale(const Size& size) const {
    return Size(Unscale(size.width), Unscale(size.height));
  }

  Rect Scale(const Rect& rect) const {
    return Rect(Scale(rect.x), Scale(rect.y),
                Scale(rect.width), Scale(rect.height));
  }

  Rect Unscale(const Rect& rect) const {
    return Rect(Unscale(rect.x), Unscale(rect.y),
                Unscale(rect.width), Unscale(rect.height));
  }

  std::string ToString() const {
    std::ostringstream oss;
    oss << "ScaleFactor(" << value << ")";
    return oss.str();
  }
};

inline std::ostream& operator<<(std::ostream& os, const ScaleFactor& sf) {
  return os << sf.ToString();
}

}  // namespace core
}  // namespace athena

#endif  // ATHENA_CORE_TYPES_H_
