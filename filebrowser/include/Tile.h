#ifndef __Tile_h__
#define __Tile_h__

#include "HierarchyNode.h"
#include "Utilities.h"

class Tile {
public:
  static const float POSITION_SMOOTH;
  static const float SIZE_SMOOTH;
  static const float ACTIVATION_SMOOTH;
  static const float GRABDELTA_SMOOTH;
  Tile();

  // getters
  Vector3 OrigPosition() const;
  Vector3 Position() const;
  Vector3 Size() const;
  float Highlight() const;
  float Activation() const;
  double LastActivationUpdateTime() const;
  float SwitchWarmupFactor() const;
  float TransitionWarmupFactor() const;
  Vector3 GrabDelta() const;
  const Vector3& TargetGrabDelta() const;
  bool IsVisible() const;

  Vector3 m_phantomPosition;

  ci::gl::TextureRef& Icon() const { return m_icon; }
  std::shared_ptr<HierarchyNode>& Node() { return m_node; }
  const std::shared_ptr<HierarchyNode>& Node() const { return m_node; }

  // setters
  void UpdateSize(const Vector3& newSize, float smooth);
  void UpdatePosition(const Vector3& newPosition, float smooth);
  void UpdateHighlight(float newHighlight, float smooth);
  void UpdateActivation(float newActivation, float smooth);
  void UpdateTargetGrabDelta(const Vector3& newGrabDelta);
  void UpdateGrabDelta(float smooth);
  void ResetActivation();
  void SetVisible(bool visible);

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

private:
  ExponentialFilter<Vector3> m_positionSmoother;
  ExponentialFilter<Vector3> m_sizeSmoother;
  ExponentialFilter<float> m_highlightSmoother;
  ExponentialFilter<float> m_activationSmoother;
  ExponentialFilter<Vector3> m_grabDeltaSmoother;
  Vector3 m_targetGrabDelta;

  mutable ci::gl::TextureRef m_icon;
  std::shared_ptr<HierarchyNode> m_node;
  bool m_visible;
  double m_visibleTime;
};

typedef std::vector<Tile, Eigen::aligned_allocator<Tile>> TileVector;
typedef std::vector<Tile *> TilePointerVector;

void SortTiles (TilePointerVector &v, std::vector<std::string> const &prioritizedKeys);

#endif
