#include "field.h"
#include "ray.h"
#include "lenses.h"
#include "obj_selection_menu.h"
#include <QPainter>
#include <QKeyEvent>
#include <QtMath>
#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QQmlEngine>
#include <QQmlContext>
#include <iostream>
#include <QQuickStyle>
#include <eigen/dense>

field::field(QWidget *parent)
    : QWidget(parent),
      corner(0,0),
      scale_step(0),
      scale(1.0),
      index_of_refr(1.0)
{
    background_color = Qt::white;

    rays_color = QColor(250, 0, 0, 128);

    background_brush = new QBrush(background_color, Qt::SolidPattern);

    rays_pen = new QPen(rays_color, 1.0, Qt::SolidLine);

    emitter_pen = new QPen(rays_color, 2.0, Qt::SolidLine);

    emitter_brush = new QBrush(rays_color, Qt::SolidPattern);

    setMouseTracking(true);

    mouse_inside = false;

    grid_visible = true;

    QQuickStyle::setStyle("Material");

    optic_options* opt = new optic_options(this);
    opt->setGeometry(0, 0, 600, 200);
    opt->engine()->rootContext()->setContextProperty("opt", opt);
    opt->setSource(QUrl("qrc:/optics_option.qml"));

    connect(opt, &optic_options::sgnFocalLength, this, [this](qreal v)
    {
        static auto cv = 0.5;
        abstract_optics* o = nullptr;
        for(auto v : optics)
        {
            if (v->who_i_am() == "Real lense")
            {
                o = v;
                break;
            }
        }
        if (o == nullptr) return;
        auto dst = (v - cv) * 100;
        cv = v;
        o->move_by(dst, 0);
        recalc();
        update();
    });

    emit something_changed();
}

field::~field()
{
    clear();
}

QList<ray *> field::get_rays() const
{
    return rays;
}

QList<abstract_optics *> field::get_optics() const
{
    return optics;
}

void field::add_ray(ray *r)
{
    if (r && (rays.indexOf(r) == -1))
        rays.append(r);
}

void field::add_optic(abstract_optics *o)
{
    if (o && (optics.indexOf(o) == -1))
        optics.append(o);
}

void field::delete_ray(ray *r)
{
    rays.removeOne(r);
}

void field::delete_optic(abstract_optics *o)
{
    optics.removeOne(o);
}

ray *field::get_ray_num(qint32 num) const
{
    return rays[num];
}

abstract_optics *field::get_optic_num(qint32 num) const
{
    return optics[num];
}

void field::clear()
{
    for (quint32 i = rays.length(); i > 0; i--)
    {
        delete rays.first();
        rays.removeFirst();
    }

    for (quint32 i = optics.length(); i > 0; i--)
    {
        delete optics.first();
        optics.removeFirst();
    }
}

void field::recalc()
{
    qint32 l = rays.length();
    for (qint32 i = 0; i < l; i++)
        recalc_ray_num(i);

    //find_focalpoint();
}

void field::find_focalpoint()
{
    auto l = rays.length();
    auto r1 = rays[0];
    auto r2 = rays[l-1];
    if (r1 == nullptr && r2 == nullptr)  return;
    // find the child ray
    auto d = 0;
    while(r1 || d<10)
    {
        if (r1->get_child() == nullptr)  break;
        r1 = r1->get_child();
        ++d;
    }
    d = 0;
    while(r2 || d<10)
    {
        if (r2->get_child() == nullptr)  break;
        r2 = r2->get_child();
        ++d;
    }
    auto p1 = r1->get_focalpoint();
    auto v1 = r1->get_direction_vect();
    auto p2 = r2->get_focalpoint();
    auto v2 = r2->get_direction_vect();
    Eigen::Matrix2f A;
    Eigen::Vector2f b;
    A << v1.x(), -v2.x(), v1.y(), -v2.y();
    b << p2.x()  -p1.x(), p2.y() -p1.y();
    auto x = A.colPivHouseholderQr().solve(b);
    auto fp = p1 + v1 * x(0);
    r1->set_focalpoint(fp);
    r2->set_focalpoint(fp);
    if (m_startcollect)
    {
        ray_path.push_back(fp);
    }

    // update the drawing - gui part
    {
        QPainterPath* q1 = r1->get_ref_path();
        if (q1 == nullptr || q1->elementCount()<1)  return;
        q1->setElementPositionAt(q1->elementCount()-1, fp.x(), fp.y());
    }
    {
        QPainterPath* q1 = r2->get_ref_path();
        if (q1 == nullptr || q1->elementCount()<1)  return;
        q1->setElementPositionAt(q1->elementCount()-1, fp.x(), fp.y());
    }
}

void field::recalc_ray_num(qint32 n)
{
    if ((n < 0) || (n >= rays.length())) return;

    ray *cur_ray = rays[n];

    while (cur_ray)
    {
        if (cur_ray->new_intersecting_object())
        {
            abstract_optics *int_opt =
                cur_ray->get_intersection_object();
            if (int_opt)
            {
                ray * new_ray = int_opt->generate_ray(cur_ray);
                cur_ray->set_child(new_ray);
            }
            else cur_ray->set_child(0);
        }
        cur_ray = cur_ray->get_child();
    }
}

void field::scale_change(qreal new_sc, QPointF *center)
{
    if ((new_sc < -50) || (new_sc > 50)) return;

    qreal old_sc = scale;
    if (!center)
    {
        center = new QPointF;
        center->setX(corner.x() + width() / (2 * scale));
        center->setY(corner.y() + height() / (2 * scale));
    }
    scale_step = new_sc;
    scale = 1*qPow(scale_base,((qreal) new_sc * 0.4));

    QPointF new_corner( *center
                        + (corner - *center) * old_sc / scale);
    corner_change(new_corner.x(), new_corner.y());

}

void field::corner_change(qreal x, qreal y)
{
    corner.setX(x);
    corner.setY(y);
}

void field::scale_turn(qreal increment, QPointF *center)
{
    scale_change(scale_step + increment, center);
}

void field::corner_turn(qreal incx, qreal incy)
{
    corner.setX(corner.x() + incx);
    corner.setY(corner.y() + incy);
}

void field::scaled_corner_turn(qreal incx, qreal incy)
{
    corner_turn(incx / scale, incy / scale);
}

qreal field::get_scale() const
{
    return scale;
}

QPointF field::get_corner() const
{
    return corner;
}

void field::set_index_of_refraction(qreal i)
{
    index_of_refr = i;
    recalc();
}

qreal field::get_index_of_refraction() const
{
    return index_of_refr;
}

QSet<qint32> field::get_highlited_rays() const
{
    return highlighted_rays;
}

QSet<qint32> field::get_highlited_optics() const
{
    return highlighted_optics;
}

QString field::who_is_optic(qint32 num) const
{
    return optics[num]->who_i_am();
}

quint32 field::rays_count() const
{
    qint32 l = rays.length();
    qint32 i = 0;
    quint32 result = 0;
    bool f = (i < l);
    ray *cur_ray;
    if (f) cur_ray = rays[0];
    while (f)
    {
        result++;
        cur_ray = cur_ray->get_child();
        if (!cur_ray)
            if (f = (++i) < l) cur_ray = rays[i];
    }

    return result;
}

void field::show_grid(bool show)
{
    if (show != grid_visible)
    {
        grid_visible = show;
        update();
        emit something_changed();
    }

}

void field::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(0, 0, width(), height(), *background_brush);

    painter.translate(-corner.x()*scale, corner.y()*scale + height());
    painter.scale(scale, -scale);

    // grid
    if (grid_visible) paintGrid(&painter);

    // rays
    qint32 l = rays.length();
    for (qint32 i = 0; i < l; i++)
        paintRay(i, &painter, highlighted_rays.contains(i));

    // optics
    l = optics.length();
    for (qint32 i = 0; i < l; i++)
        paintOptic(i, &painter, highlighted_optics.contains(i));

    if (m_startpaint)
    {
        QPen raypen;
        raypen.setColor(Qt::darkRed);
        painter.setPen(raypen);
        for(int i = 1; i<ray_path.size(); ++i)
        {
            painter.drawLine(ray_path.at(i-1), ray_path.at(i));
        }
    }
}

void field::keyPressEvent(QKeyEvent *ke)
{
    switch (ke->key())
    {
//    case Qt::Key_Up:
//        scaled_corner_turn(0, height() * turn_koeff);
//        update();
//        emit something_changed();
//        break;

//    case Qt::Key_Down:
//        scaled_corner_turn(0, -height() * turn_koeff);
//        update();
//        emit something_changed();
//        break;
//    case Qt::Key_Left:
//        scaled_corner_turn(-width() * turn_koeff, 0);
//        update();
//        emit something_changed();
//        break;
    case Qt::Key_1:
        ray_path.clear();
        recalc();
        update();
        emit something_changed();
        break;
    case Qt::Key_P:
        m_startcollect = !m_startcollect;
        recalc();
        update();
        emit something_changed();
        break;
    case Qt::Key_C:
        m_startpaint = !m_startpaint;
        update();
        break;
    case Qt::Key_Space:
        // print out coordinates of the optics element and the scene
        for(auto o : optics)
        {
            std::cout << o->who_i_am().toStdString() << ", " << o->get_position().toStdString() << std::endl;
        }

    default:
        break;
    }

}

void field::enterEvent(QEvent *)
{
    mouse_inside = true;
}

void field::leaveEvent(QEvent *)
{
    mouse_inside = false;
}

void field::wheelEvent(QWheelEvent *we)
{
    switch (we->modifiers())
    {
    case Qt::ControlModifier:
        if (!highlighted_optics.isEmpty())
        {
            optics[highlighted_optics.values().front()]->rot_by(0.01*we->delta());
            recalc();
        }
        update();
        emit something_changed();
        return;
    }

    QPointF *center = new QPointF;
    center->setX(corner.x() + we->x() / scale);
    center->setY(corner.y() + (height() - we->y()) / scale);
    auto v = 0.01*we->delta();
    scale_turn(v, center);
    update();
    emit something_changed();
}

void field::mouseMoveEvent(QMouseEvent *me)
{
    QPointF delta;
    delta.setX( me->x() - mouse_click_pos.x() );
    delta.setY( me->y() - mouse_click_pos.y() );
    delta /= scale;
    switch (me->buttons())
    {
    case Qt::LeftButton:
        if (highlighted_optics.isEmpty())
        {
            corner_turn(- delta.x(), delta.y());
        }
        else
        {
            optics[highlighted_optics.values().front()]->move_by(delta.x(), delta.y());
            recalc();
        }
        update();
        emit something_changed();
        break;

    default:
        if (highlight_changed()) update();
        break;
    }

    mouse_click_pos.setX(me->x());
    mouse_click_pos.setY(me->y());
}

void field::mousePressEvent(QMouseEvent *me)
{
    mouse_click_pos.setX(me->x());
    mouse_click_pos.setY(me->y());
    switch(me->buttons())
    {
    case Qt::LeftButton:
        setCursor(Qt::ClosedHandCursor);
        break;
    }
}

void field::mouseReleaseEvent(QMouseEvent *)
{
    unsetCursor();
}

void field::highlight_ray(qint32 num)
{
    if (num < rays.length())
    {
        highlighted_rays.clear();
        highlighted_optics.clear();
        highlighted_rays.insert(num);
        update();
    }
}

void field::highlight_optic(qint32 num)
{
    if ((num >= 0) && (num < optics.length()))
    {
        highlighted_optics.clear();
        highlighted_rays.clear();
        highlighted_optics.insert(num);
        update();
    }
}

void field::paintRay(qint32 num, QPainter *painter,
                     bool selected) const
{
    ray *cur_ray = rays[num];
    while (cur_ray)
    {
        if (!cur_ray->get_parent())
        {
            QPen e_pen = cur_ray->get_emitter_pen();
            qreal r = cur_ray->get_emitter_radius() / scale;
            e_pen.setWidthF(e_pen.widthF()/scale);
            painter->setBrush(cur_ray->get_emitter_brush());
            painter->setPen(e_pen);
            painter->drawEllipse(cur_ray->get_emitter_pos(),r, r);
        }

        QPen r_pen = cur_ray->get_pen();
        if (selected)
            r_pen.setWidthF(2 * (1 + r_pen.widthF())/scale);
        else r_pen.setWidthF(r_pen.widthF()/scale);
        painter->setPen(r_pen);
        painter->drawPath(cur_ray->get_path());

        cur_ray = cur_ray->get_child();
    }

}

void field::paintOptic(qint32 num, QPainter *painter,
                       bool selected) const
{
    abstract_optics *cur_opt = optics[num];
    QPen o_pen = cur_opt->get_pen();
    QBrush o_brush = cur_opt->get_brush();
    if (selected)
    {
        o_pen.setWidthF(2 * (1 + o_pen.widthF()) / scale);
        QColor color = o_brush.color();
        color.setAlphaF(0.5 + 0.5 * color.alphaF());
        o_brush.setColor(color);
    }
    else o_pen.setWidthF(o_pen.widthF() / scale);

    painter->setPen(o_pen);
    painter->setBrush(o_brush);
    painter->drawPath(cur_opt->get_outline());
}

void field::paintGrid(QPainter *painter) const
{
    static const qreal ln_10 = 2.302585092994046;
    qreal lines_dist =  qPow(10.0, 2 - (qint32) (qLn(scale)/ln_10));
    qint32 h_lines_count = 3 + height() / (scale * lines_dist);
    qreal first_line_y = ( (int) (corner.y() / lines_dist) - 1)
                         * lines_dist;
    qint32 v_lines_count = 3 + width() / (scale * lines_dist);
    qreal first_line_x = ( (int) (corner.x() / lines_dist) - 1)
                         * lines_dist;

    QColor grid_color;
    grid_color.setRgb(32,32,32, 10);
    painter->setPen(QPen(grid_color, 0, Qt::SolidLine));
    for (qint32 i = 0; i < h_lines_count * 5; i++)
    {
        qreal y = first_line_y + i * lines_dist * 0.2;
        painter->drawLine(QPointF(corner.x(), y),
                          QPointF(corner.x() + width() / scale, y));
    }

    painter->setPen(QPen(grid_color, 0, Qt::SolidLine));
    for (qint32 i = 0; i < v_lines_count * 5; i++)
    {
        qreal x = first_line_x + i * lines_dist * 0.2;
        painter->drawLine(QPointF(x, corner.y()),
                          QPointF(x, corner.y() + height() / scale));
    }

    grid_color.setRgb(150, 150, 150, 10);
    painter->setPen(QPen(grid_color, 2/scale, Qt::SolidLine));
    for (qint32 i = 0; i < h_lines_count; i++)
    {
        qreal y = first_line_y + i * lines_dist;
        painter->drawLine(QPointF(corner.x(), y),
                          QPointF(corner.x() + width() / scale, y));
    }

    for (qint32 i = 0; i < v_lines_count; i++)
    {
        qreal x = first_line_x + i * lines_dist;
        painter->drawLine(QPointF(x, corner.y()),
                          QPointF(x, corner.y() + height() / scale));
    }
}

bool field::highlight_changed()
{
    vector2D mouse_pos;
    mouse_pos.setX(corner.x() + mouse_click_pos.x() / scale);
    mouse_pos.setY(corner.y() +
                   (height() - mouse_click_pos.y()) / scale);
    bool result = false;
    // rays
    qint32 l = rays.length();
    for (qint32 i = 0; i < l; i++)
    {
        bool flag1 =
            rays[i]->get_distance_to_point(mouse_pos) * scale
            <= mouse_select_distance;
        bool flag2 = highlighted_rays.contains(i);
        if (flag1 != flag2)
        {
            result = true;
            if (flag1) highlighted_rays.insert(i);
            else highlighted_rays.remove(i);
        }
    }

    // optics
    l = optics.length();
    for (qint32 i = 0; i < l; i++)
    {
        bool flag1 =
            optics[i]->get_distance_to_point(mouse_pos) * scale
            <= mouse_select_distance;
        bool flag2 = highlighted_optics.contains(i);
        if (flag1 != flag2)
        {
            result = true;
            if (flag1) highlighted_optics.insert(i);
            else highlighted_optics.remove(i);
        }
    }
    return result;
}
