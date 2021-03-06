#ifndef __Utilities_h__
#define __Utilities_h__

template <class T>
static inline T SmootherStep(const T& x) {
  // x is blending parameter between 0 and 1
  return x*x*x*(x*(x*6 - 15) + 10);
}

inline ci::Vec3f ToVec3f(const Vector3& vec) {
  return ci::Vec3f(static_cast<float>(vec.x()), static_cast<float>(vec.y()), static_cast<float>(vec.z()));
}

static const int TIME_STAMP_TICKS_PER_SEC = 1000000;
static const double TIME_STAMP_SECS_TO_TICKS  = static_cast<double>(TIME_STAMP_TICKS_PER_SEC);
static const double TIME_STAMP_TICKS_TO_SECS  = 1.0/TIME_STAMP_SECS_TO_TICKS;

static const float RADIANS_TO_DEGREES = static_cast<float>(180.0 / M_PI);
static const float DEGREES_TO_RADIANS = static_cast<float>(M_PI / 180.0);

#endif
