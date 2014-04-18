#include "StdAfx.h"
#include "Render.h"
#include "Utilities.h"
#include "Globals.h"

Render::Render() {
}

void Render::update_background(const ci::Surface8u& surface) {
  m_background = ci::gl::Texture::create(surface);
}

void Render::draw(const View& view) const {
  if (m_background) {
    ci::gl::draw(m_background);
  }
  m_camera.lookAt(ToVec3f(view.Position()), ToVec3f(view.LookAt()), ToVec3f(view.Up()));
  m_camera.setPerspective(view.FOV(), static_cast<float>(Globals::aspectRatio), view.Near(), view.Far());
  ci::gl::setMatrices(m_camera);

  const TileVector& tiles = view.Tiles();
  for (TileVector::const_iterator it = tiles.begin(); it != tiles.end(); ++it) {
    drawTile(*it);
  }
}

void Render::drawTile(const Tile& tile) const {
  if (!tile.m_node) {
    return;
  }
  glPushMatrix();
  glTranslated(tile.m_position.x(), tile.m_position.y(), tile.m_position.z());

  const float halfWidth = static_cast<float>(tile.m_size.x()/2.0f);
  const float halfHeight = static_cast<float>(tile.m_size.y()/2.0f);
  const ci::Rectf rect(-halfWidth, -halfHeight, halfWidth, halfHeight);

  if (!tile.m_icon) {
    ci::Surface8u icon = tile.m_node->icon();
    if (icon) {
      tile.m_icon = ci::gl::Texture::create(icon);
    }
  }
  if (tile.m_icon) {
    // draw the icon
    glPushMatrix();
    glScaled(1, -1, 1);
    ci::gl::draw(tile.m_icon, rect);
    glPopMatrix();
  } else {
    // draw border
    ci::gl::color(ci::ColorA(0.5f, 0.5f, 0.5f));
    ci::gl::drawSolidRoundedRect(rect, 2.0, 10);
    ci::gl::color(ci::ColorA::white());
    ci::gl::drawStrokedRoundedRect(rect, 2.0, 10);
  }

  // draw text
  static const ci::ColorA nameColor = ci::ColorA::white();
  static const ci::ColorA shadowColor = ci::ColorA(0.1f, 0.1f, 0.1f);
  static const ci::Vec2f shadowOffset = ci::Vec2f(5.0, 7.0f);
  glPushMatrix();
  const std::string name = tile.m_node->get_metadata_as<std::string>("name");
  const ci::Vec2f nameSize = Globals::fontRegular->measureString(name);
  const ci::Rectf nameRect(-nameSize.x/2.0f, 0.0f, nameSize.x/2.0f + shadowOffset.x, 100.0f);

  const double TEXT_SCALE = std::min(0.03, tile.m_size.x() / nameSize.x);
  static const double NAME_OFFSET = 1.0;
  glTranslated(0.0, -tile.m_size.y()/2.0 - NAME_OFFSET, 0.0);
  glScaled(TEXT_SCALE, -TEXT_SCALE, TEXT_SCALE);
  glTranslated(0, -Globals::FONT_SIZE/2.0, 0);
  ci::gl::color(shadowColor);
  Globals::fontRegular->drawString(name, nameRect, shadowOffset);
  ci::gl::color(nameColor);
  Globals::fontRegular->drawString(name, nameRect);
  glPopMatrix();

  glPopMatrix();
}
