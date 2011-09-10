#include "draw_widget.hpp"
#include "proxystyle.hpp"
#include "slider_ctrl.hpp"

#include "../storage/storage.hpp"

#include "../platform/settings.hpp"

#include <QtGui/QMouseEvent>

#include "../map/framework_factory.hpp"
#include "../base/start_mem_debug.hpp"

using namespace storage;

namespace qt
{
  DrawWidget::DrawWidget(QWidget * pParent, Storage & storage)
    : base_type(pParent),
      m_handle(new qt::WindowHandle(this)),
      m_framework(FrameworkFactory<model_t>::CreateFramework(m_handle, 0)),
      m_isDrag(false),
      m_redrawInterval(100),
      m_pScale(0)
  {
    m_framework->InitStorage(storage);
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(ScaleTimerElapsed()));
  }

  void DrawWidget::PrepareShutdown()
  {
    m_framework->PrepareToShutdown();
  }

  void DrawWidget::SetScaleControl(QScaleSlider * pScale)
  {
    m_pScale = pScale;

    connect(m_pScale, SIGNAL(actionTriggered(int)), this, SLOT(ScaleChanged(int)));
  }

  void DrawWidget::UpdateNow()
  {
    m_framework->Invalidate();
  }

  bool DrawWidget::LoadState()
  {
    pair<int, int> widthAndHeight;
    if (!Settings::Get("DrawWidgetSize", widthAndHeight))
      return false;

    m_framework->OnSize(widthAndHeight.first, widthAndHeight.second);

    if (!m_framework->LoadState())
      return false;
    //m_framework.UpdateNow();

    UpdateScaleControl();
    return true;
  }

  void DrawWidget::SaveState()
  {
    pair<int, int> widthAndHeight(width(), height());
    Settings::Set("DrawWidgetSize", widthAndHeight);

    m_framework->SaveState();
  }

  //void DrawWidget::ShowFeature(Feature const & p)
  //{
  //  m_framework.ShowFeature(p);
  //}

  void DrawWidget::OnEnableMyPosition(LocationRetrievedCallbackT observer)
  {
    m_framework->StartLocationService(observer);
  }

  void DrawWidget::OnDisableMyPosition()
  {
    m_framework->StopLocationService();
  }

  void DrawWidget::MoveLeft()
  {
    m_framework->Move(math::pi, 0.5);
    emit ViewportChanged();
  }

  void DrawWidget::MoveRight()
  {
    m_framework->Move(0.0, 0.5);
    emit ViewportChanged();
  }

  void DrawWidget::MoveUp()
  {
    m_framework->Move(math::pi/2.0, 0.5);
    emit ViewportChanged();
  }

  void DrawWidget::MoveDown()
  {
    m_framework->Move(-math::pi/2.0, 0.5);
    emit ViewportChanged();
  }

  void DrawWidget::ScalePlus()
  {
    m_framework->Scale(2.0);
    UpdateScaleControl();
    emit ViewportChanged();
  }

  void DrawWidget::ScaleMinus()
  {
    m_framework->Scale(0.5);
    UpdateScaleControl();
    emit ViewportChanged();
  }

  void DrawWidget::ScalePlusLight()
  {
    m_framework->Scale(1.5);
    UpdateScaleControl();
    emit ViewportChanged();
  }

  void DrawWidget::ScaleMinusLight()
  {
    m_framework->Scale(2.0/3.0);
    UpdateScaleControl();
    emit ViewportChanged();
  }

  void DrawWidget::ShowAll()
  {
    m_framework->ShowAll();
    UpdateScaleControl();
    emit ViewportChanged();
  }

  void DrawWidget::Repaint()
  {
    m_framework->Invalidate();
  }

  void DrawWidget::ScaleChanged(int action)
  {
    if (action != QAbstractSlider::SliderNoAction)
    {
      double const factor = m_pScale->GetScaleFactor();
      if (factor != 1.0)
      {
        m_framework->Scale(factor);
        emit ViewportChanged();
      }
    }
  }

  void DrawWidget::initializeGL()
  {
    widget_type::initializeGL();
    m_handle->setRenderContext(renderContext());
    //m_handle->setDrawer(GetDrawer());
    m_framework->InitializeGL(renderContext(), resourceManager());
  }

  void DrawWidget::DoDraw(shared_ptr<drawer_t> p)
  {
    m_framework->BeginPaint();
    shared_ptr<PaintEvent> paintEvent(new PaintEvent(p.get()));
    m_framework->DoPaint(paintEvent);
    m_framework->EndPaint();
  }

  void DrawWidget::DoResize(int w, int h)
  {
    m_framework->OnSize(w, h);
    m_framework->Invalidate();
    UpdateScaleControl();
    emit ViewportChanged();
  }

  namespace
  {
    DragEvent get_drag_event(QMouseEvent * e)
    {
      QPoint const p = e->pos();
      return DragEvent(DragEvent(p.x(), p.y()));
    }
  }

  void DrawWidget::mousePressEvent(QMouseEvent * e)
  {
    base_type::mousePressEvent(e);

    if (e->button() == Qt::LeftButton)
    {
      m_framework->StartDrag(get_drag_event(e));

      setCursor(Qt::CrossCursor);
      m_isDrag = true;
    }
  }

  void DrawWidget::mouseDoubleClickEvent(QMouseEvent * e)
  {
    base_type::mouseDoubleClickEvent(e);

    if (e->button() == Qt::LeftButton)
    {
      StopDragging(e);

      m_framework->ScaleToPoint(ScaleToPointEvent(e->pos().x(), e->pos().y(), 1.5));

      UpdateScaleControl();
      emit ViewportChanged();
    }
  }

  void DrawWidget::mouseMoveEvent(QMouseEvent * e)
  {
    base_type::mouseMoveEvent(e);

    if (m_isDrag)
      m_framework->DoDrag(get_drag_event(e));
  }

  void DrawWidget::mouseReleaseEvent(QMouseEvent * e)
  {
    base_type::mouseReleaseEvent(e);

    StopDragging(e);
    emit ViewportChanged();
  }

  void DrawWidget::StopDragging(QMouseEvent * e)
  {
    if (m_isDrag && e->button() == Qt::LeftButton)
    {
      m_framework->StopDrag(get_drag_event(e));

      setCursor(Qt::ArrowCursor);
      m_isDrag = false;
    }
  }

  void DrawWidget::ScaleTimerElapsed()
  {
    m_timer->stop();
  }

  void DrawWidget::wheelEvent(QWheelEvent * e)
  {
    if (!m_isDrag)
    {
      /// if we are inside the timer, cancel it
      if (m_timer->isActive())
        m_timer->stop();

      m_timer->start(m_redrawInterval);
      //m_framework->Scale(exp(e->delta() / 360.0));
      m_framework->ScaleToPoint(ScaleToPointEvent(e->pos().x(), e->pos().y(), exp(e->delta() / 360.0)));
      UpdateScaleControl();
      emit ViewportChanged();
    }
  }

  void DrawWidget::UpdateScaleControl()
  {
    if (m_pScale)
    {
      // don't send ScaleChanged
      m_pScale->SetPosWithBlockedSignals(m_framework->GetCurrentScale());
    }
  }

  void DrawWidget::Search(const string & text, SearchCallbackT callback)
  {
    m_framework->Search(text, callback);
  }

  void DrawWidget::ShowFeature(m2::RectD const & rect)
  {
    m_framework->ShowRect(rect);
    UpdateScaleControl();
  }
}
