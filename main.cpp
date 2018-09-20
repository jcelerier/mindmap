#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QDomDocument>
#include <QDomNode>
#include <QFile>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWheelEvent>

#include <variant>

#include <unordered_map>
#include <unordered_set>
namespace std
{
template <typename T, typename U>
struct hash<std::pair<T, U>>
{
  auto operator()(const std::pair<T, U>& h) const noexcept
  {
    return hash<T>{}(h.first) ^ hash<T>{}(h.second);
  }
};
}

struct NodeInfo
{
  int id = 0;
  QRectF rect{};
  QString label;
};

struct LinkInfo
{
  int from = 0;
  int to = 0;
};

using VueElement = std::variant<NodeInfo, LinkInfo>;

static VueElement parseChild(const QDomNode& node)
{
  VueElement v;

  int64_t id{};
  double x{}, y{}, w{}, h{};
  QString label;
  enum
  {
    Node,
    Link
  } kind{};

  QDomNamedNodeMap attr = node.attributes();
  for (int i = 0; i < attr.count(); i++)
  {
    QDomNode item = attr.item(i);
    const auto& name = item.nodeName();
    if (name == "label")
    {
      label = item.childNodes().at(0).toText().data();
    }
    else if (name == "ID")
    {
      id = item.childNodes().at(0).toText().data().toInt();
    }
    else if (name == "x")
    {
      x = item.childNodes().at(0).toText().data().toDouble();
    }
    else if (name == "y")
    {
      y = item.childNodes().at(0).toText().data().toDouble();
    }
    else if (name == "width")
    {
      w = item.childNodes().at(0).toText().data().toDouble();
    }
    else if (name == "height")
    {
      h = item.childNodes().at(0).toText().data().toDouble();
    }
    else if (name == "xsi:type")
    {
      if (item.childNodes().at(0).toText().data() == "link")
        kind = Link;
    }
  }

  switch (kind)
  {
    case Link:
    {
      LinkInfo l;
      auto e = node.firstChildElement();
      while (!e.isNull())
      {
        if (e.nodeName() == "ID1")
        {
          l.from = e.firstChild().toText().data().toInt();
        }
        else if (e.nodeName() == "ID2")
        {
          l.to = e.firstChild().toText().data().toInt();
        }
        e = e.nextSiblingElement();
      }

      v = std::move(l);
      break;
    }
    case Node:
    {
      NodeInfo n;
      n.id = id;
      n.rect = {x, y, w, h};
      n.label = label;
      v = std::move(n);
      break;
    }
    default:
      break;
  }
  return v;
}

struct Node;
struct Link final : QGraphicsLineItem
{
  const int64_t from;
  const int64_t to;
  Link(qreal x1, qreal y1, qreal x2, qreal y2, int64_t from, int64_t to)
      : QGraphicsLineItem{x1, y1, x2, y2}, from{from}, to{to}
  {
  }

  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget)
  {
    painter->setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing);
    QGraphicsLineItem::paint(painter, option, widget);
  }
};

struct Canvas : QObject
{
  std::unordered_map<int64_t, Node*> nodes;
  std::vector<Link*> links;
};

struct Node final : QGraphicsRectItem
{
public:
  const int64_t id;
  Canvas& canvas;
  Node(int64_t i, QRectF rect, QString text, Canvas& c)
      : QGraphicsRectItem{{0, 0, rect.width(), rect.height()}}
      , id{i}
      , canvas{c}
  {
    setBrush(QColor(qRgb(245, 245, 225)));
    setPen(QColor(Qt::black));
    setZValue(2);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemSendsScenePositionChanges);

    auto child = new QGraphicsTextItem{text, this};
    child->setFont(QFont("Arial", 9));
    child->setParentItem(this);
  }

  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget)
  {
    painter->setRenderHints(
        QPainter::Antialiasing | QPainter::TextAntialiasing);
    QGraphicsRectItem::paint(painter, option, widget);
  }

  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) override
  {
    if (change == GraphicsItemChange::ItemScenePositionHasChanged)
    {
      for (Link* link : canvas.links)
      {
        if (link->from == id)
        {
          QLineF l = link->line();
          l.setP1(value.toPointF() + boundingRect().center());
          link->setLine(l);
        }
        else if (link->to == id)
        {
          QLineF l = link->line();
          l.setP2(value.toPointF() + boundingRect().center());
          link->setLine(l);
        }
      }
    }
    return QGraphicsRectItem::itemChange(change, value);
  }
};

class ZoomView final : public QGraphicsView
{
public:
  using QGraphicsView::QGraphicsView;
  void wheelEvent(QWheelEvent* event) override
  {
    if (event->modifiers() & Qt::ControlModifier)
    {
      // zoom
      const ViewportAnchor anchor = transformationAnchor();
      setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
      int angle = event->angleDelta().y();
      qreal factor;

      if (angle > 0)
      {
        factor = 1.1 - std::clamp(0.1 / std::abs(angle), 0., 0.1);
      }
      else
      {
        factor = 0.9 + std::clamp(0.1 / std::abs(angle), 0., 0.1);
      }

      double curscale = transform().m11() * factor;
      if (curscale > 0.01 && curscale < 4)
      {
        scale(factor, factor);
      }
      setTransformationAnchor(anchor);
    }
    else
    {
      QGraphicsView::wheelEvent(event);
    }
  }

  void drawBackground(QPainter* painter, const QRectF& s) override
  {
    constexpr auto rgb = [](int r, int g, int b, int a = 255) {
      return QColor::fromRgb(r, g, b, a);
    };
    QBrush b = QBrush(rgb(215, 214, 208).lighter(115), Qt::CrossPattern);
    QBrush bg = QBrush(rgb(215, 214, 208).lighter(130));

    b.setTransform(
        QTransform(painter->worldTransform().inverted()).scale(2, 2));
    painter->setBackground(bg);
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setBrush(b);
    painter->fillRect(s, b);
  }
};

int main(int argc, char* argv[])
{
  QApplication a(argc, argv);

  QGraphicsScene scene;
  ZoomView view;
  view.setScene(&scene);

  QFile f("/home/jcelerier/mindmap2017.vue");
  f.open(QIODevice::ReadOnly);
  QByteArray dat = f.readAll();
  int cur_idx = 0;
  for (int i = 0; i < 5; i++)
  {
    cur_idx = dat.indexOf('\n', cur_idx + 1);
  }
  dat.remove(0, cur_idx + 1);

  QDomDocument r;
  r.setContent(dat);

  QDomElement doc = r.documentElement();

  struct
  {
    std::vector<NodeInfo> nodes;
    std::vector<LinkInfo> links;

    void operator()(NodeInfo&& n)
    {
      nodes.push_back(std::move(n));
    }
    void operator()(LinkInfo&& n)
    {
      links.push_back(std::move(n));
    }
  } objects;

  for (int i = 0; i < doc.childNodes().count(); i++)
  {
    const QDomNode& node = doc.childNodes().at(i);
    if (node.nodeName() == "child")
      std::visit(objects, parseChild(node));
  }

  Canvas sc;
  for (const NodeInfo& obj : objects.nodes)
  {
    if (!obj.label.isEmpty())
    {
      auto item = new Node(obj.id, obj.rect, obj.label, sc);
      sc.nodes[obj.id] = item;
      item->setPos(obj.rect.x(), obj.rect.y());
      scene.addItem(item);
    }
  }

  for (const LinkInfo& obj : objects.links)
  {
    auto it1 = sc.nodes.find(obj.from);
    auto it2 = sc.nodes.find(obj.to);
    if (it1 != sc.nodes.end() && it2 != sc.nodes.end())
    {
      Node* from = it1->second;
      Node* to = it2->second;
      auto start = from->mapToScene(from->boundingRect().center());
      auto end = to->mapToScene(to->boundingRect().center());
      auto line
          = new Link(start.x(), start.y(), end.x(), end.y(), obj.from, obj.to);
      sc.links.push_back(line);
      scene.addItem(line);
    }
  }

  view.show();
  return a.exec();
}
