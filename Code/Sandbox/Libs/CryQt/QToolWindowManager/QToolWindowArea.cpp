// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QToolWindowArea.h"
#include "QToolWindowManager.h"
#include "QToolWindowWrapper.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QStyleOptionToolBar>
#include <QStylePainter>
#include <QTabBar>

QToolWindowArea::QToolWindowArea(QToolWindowManager* manager, QWidget* parent /*= 0*/)
	: QTabWidget(parent),
	m_manager(manager),
	m_tabDragCanStart(false),
	m_areaDragCanStart(false)
{
	setTabBar(new QToolWindowTabBar(this));
	setMovable(true);
	setTabShape(QTabWidget::Rounded);
	setDocumentMode(m_manager->config().value(QTWM_AREA_DOCUMENT_MODE, true).toBool());
	m_useCustomTabCloseButton = m_manager->config().value(QTWM_AREA_TABS_CLOSABLE, false).toBool() && m_manager->config().contains(QTWM_TAB_CLOSE_ICON);
	setTabsClosable(m_manager->config().value(QTWM_AREA_TABS_CLOSABLE, false).toBool() && !m_useCustomTabCloseButton);
	setTabPosition((QTabWidget::TabPosition)m_manager->config().value(QTWM_AREA_TAB_POSITION, QTabWidget::North).toInt());

	tabBar()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	setFocusPolicy(Qt::StrongFocus);
	bool areaUseImageHandle = m_manager->config().value(QTWM_AREA_IMAGE_HANDLE, false).toBool();
	m_pTabFrame = new QToolWindowSingleTabAreaFrame(manager, this);
	m_pTabFrame->hide();

	if (m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool())
	{
		QLabel* corner = new QLabel(this);
		corner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
		corner->setAttribute(Qt::WA_TranslucentBackground);
		setCornerWidget(corner);
		if (areaUseImageHandle)
		{
			QPixmap corner_img;
			corner_img.load(manager->config().value(QTWM_DROPTARGET_COMBINE, ":/QtDockLibrary/gfx/drag_handle.png").toString());
			corner->setPixmap(corner_img);
		}
		else
		{
			corner->setFixedWidth(8);
			QPixmap corner_img(corner->size());
			corner_img.fill(Qt::transparent);

			QStyleOptionToolBar option;
			option.initFrom(tabBar());
			option.state |= QStyle::State_Horizontal;
			option.lineWidth = tabBar()->style()->pixelMetric(QStyle::PM_ToolBarFrameWidth, 0, tabBar());
			option.features = QStyleOptionToolBar::Movable;
			option.toolBarArea = Qt::NoToolBarArea;
			option.direction = Qt::RightToLeft;
			option.rect = corner_img.rect();
			option.rect.moveTo(0, 0);

			QPainter painter(&corner_img);
			tabBar()->style()->drawPrimitive(QStyle::PE_IndicatorToolBarHandle, &option, &painter, corner);
			corner->setPixmap(corner_img);
		}
		corner->setCursor(Qt::CursorShape::OpenHandCursor);
		corner->installEventFilter(this);
	}
	tabBar()->installEventFilter(this);
	m_pTabFrame->installEventFilter(this);
	m_pTabFrame->m_pCaption->installEventFilter(this);

	tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
	connect(tabBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
	connect(tabBar(), SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
}

QToolWindowArea::~QToolWindowArea()
{
	if (m_manager)
	{
		m_manager->removeArea(this);
		m_manager = nullptr;
	}
}

QPushButton* QToolWindowArea::createCloseButton()
{
	QPushButton* result = new QPushButton(this);
	result->setIcon(getIcon(m_manager->config(), QTWM_TAB_CLOSE_ICON, QIcon(":/QtDockLibrary/gfx/close.png")));
	connect(result, SIGNAL(clicked()), this, SLOT(tabCloseButtonClicked()));
	return result;
}

void QToolWindowArea::addToolWindow(QWidget* toolWindow, int index /*= -1 */)
{
	addToolWindows(QList<QWidget*>() << toolWindow, index);
}

void QToolWindowArea::addToolWindows(const QList<QWidget*>& toolWindows, int index /*= -1 */)
{
	int newIndex = index;
	foreach(QWidget * toolWindow, toolWindows)
	{
		if (m_manager->config().value(QTWM_AREA_TAB_ICONS, false).toBool())
		{
			if (index < 0)
			{
				newIndex = addTab(toolWindow, toolWindow->windowIcon(), toolWindow->windowTitle());
			}
			else
			{
				newIndex = insertTab(newIndex, toolWindow, toolWindow->windowIcon(), toolWindow->windowTitle()) + 1;
			}
		}
		else
		{
			if (index < 0)
			{
				newIndex = addTab(toolWindow, toolWindow->windowTitle());
			}
			else
			{
				newIndex = insertTab(newIndex, toolWindow, toolWindow->windowTitle()) + 1;
			}
		}

		if (m_useCustomTabCloseButton)
		{
			tabBar()->setTabButton(newIndex, QTabBar::ButtonPosition::RightSide, createCloseButton());
		}

		connect(toolWindow, &QWidget::windowTitleChanged, this, [this, toolWindow](const QString& title)
		{
			int index = indexOf(toolWindow);
			if (index >= 0)
			{
			  setTabText(index, title);
			}
			if (count() == 1)
			{
			  setWindowTitle(title);
			}
		});
		connect(toolWindow, &QWidget::windowIconChanged, this, [this, newIndex, toolWindow](const QIcon& icon)
		{
			if (indexOf(toolWindow) >= 0)
			{
			  setTabIcon(newIndex, icon);
			}
			if (count() == 1)
			{
			  setWindowIcon(icon);
			}
		});
	}
	setCurrentWidget(toolWindows.first());
}

void QToolWindowArea::tabCloseButtonClicked()
{
	int index = -1;
	QObject* o = sender();
	for (int i = 0; i < count(); i++)
	{
		if (o == tabBar()->tabButton(i, QTabBar::RightSide))
		{
			index = i;
			break;
		}
	}
	if (index != -1)
	{
		tabBar()->tabCloseRequested(index);
	}
}

void QToolWindowArea::closeTab(int index)
{
	m_manager->releaseToolWindow(widget(index), true);
}

void QToolWindowArea::removeToolWindow(QWidget* toolWindow)
{
	toolWindow->disconnect(this);
	const int i = indexOf(toolWindow);
	if (i != -1)
	{
		removeTab(i);
	}
	else if (m_pTabFrame->contents() == toolWindow)
	{
		const int i = indexOf(m_pTabFrame);
		if (i != -1)
		{
			removeTab(i);
		}
		m_pTabFrame->setContents(nullptr);
	}
}

QList<QWidget*> QToolWindowArea::toolWindows()
{
	QList<QWidget*> result;
	for (int i = 0; i < count(); i++)
	{
		QWidget* w = widget(i);
		if (w == m_pTabFrame)
		{
			w = m_pTabFrame->contents();
		}
		result << w;
	}
	return result;
}

bool QToolWindowArea::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pObject == tabBar() || pObject == cornerWidget() || pObject == m_pTabFrame || pObject == m_pTabFrame->m_pCaption)
	{
		if (pEvent->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(pEvent);
			if ((pObject == m_pTabFrame && m_pTabFrame->m_pCaption->rect().contains(me->pos())) || pObject == m_pTabFrame->m_pCaption || (pObject == tabBar() && tabBar()->tabAt(static_cast<QMouseEvent*>(pEvent)->pos()) >= 0))
			{
				m_tabDragCanStart = true;
			}
			else if (m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool() && (pObject == cornerWidget() || (pObject == tabBar() && cornerWidget()->isVisible() && m_manager->config().value(QTWM_AREA_EMPTY_SPACE_DRAG, false).toBool())))
			{
				m_areaDragCanStart = true;
			}
			if (m_manager->isMainWrapper(parentWidget()))
			{
				m_areaDragCanStart = false;
				if (count() == 1)
				{
					m_tabDragCanStart = false;
				}
			}
		}
		else if (pEvent->type() == QEvent::MouseMove)
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(pEvent);
			if (m_tabDragCanStart)
			{
				if (tabBar()->rect().contains(me->pos()) || (m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool() && cornerWidget()->rect().contains(me->pos())))
				{
					return false;
				}
				if (qApp->mouseButtons() != Qt::LeftButton)
				{
					return false;
				}
				QWidget* toolWindow = currentWidget();
				if (qobject_cast<QToolWindowSingleTabAreaFrame*>(toolWindow) == m_pTabFrame)
				{
					toolWindow = m_pTabFrame->contents();
				}
				m_tabDragCanStart = false;
				//stop internal tab drag in QTabBar
				QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, me->pos(), Qt::LeftButton, Qt::LeftButton, 0);
				qApp->sendEvent(tabBar(), releaseEvent);
				m_manager->startDrag(QList<QWidget*>() << toolWindow, this);
				releaseMouse();
			}
			else if (m_areaDragCanStart)
			{
				if (qApp->mouseButtons() == Qt::LeftButton && !(m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool() && cornerWidget()->rect().contains(mapFromGlobal(QCursor::pos()))))
				{
					QList<QWidget*> toolWindows;
					for (int i = 0; i < count(); i++)
					{
						QWidget* toolWindow = widget(i);
						if (qobject_cast<QToolWindowSingleTabAreaFrame*>(toolWindow) == m_pTabFrame)
						{
							toolWindow = m_pTabFrame->contents();
						}
						toolWindows << toolWindow;
					}
					m_areaDragCanStart = false;
					QMouseEvent* releaseEvent = new QMouseEvent(QEvent::MouseButtonRelease, me->pos(), Qt::LeftButton, Qt::LeftButton, 0);
					if (cornerWidget())
					{
						qApp->sendEvent(cornerWidget(), releaseEvent);
					}
					m_manager->startDrag(toolWindows, this);
					releaseMouse();
				}
			}
		}
	}
	return QTabWidget::eventFilter(pObject, pEvent);
}

bool QToolWindowArea::event(QEvent* event)
{
	//Here we deal with area drag, drag can happen only if the QTWM_AREA_SHOW_DRAG_HANDLE is disabled and we are not in a floating window. Also dragging the tool windows is not allowed
	if (!m_areaDragCanStart && event->type() == QEvent::MouseButtonPress && !m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool()) // in this case we are dragging the tab frame
	{
		//If in a floating wrapper then we cannot drag
		bool floatingWrapper = false;
		IToolWindowWrapper* pWrapper = wrapper();
		if (pWrapper && m_manager->isFloatingWrapper(pWrapper->getWidget()))
		{
			floatingWrapper = true;
		}

		//event with a mouse can also be triggered by bubbling, to avoid that we check that the mouse is outside of the child widget
		if (!floatingWrapper && currentWidget() && !currentWidget()->rect().contains(currentWidget()->mapFromGlobal(QCursor::pos())))
		{
			m_areaDragCanStart = true;
		}
	}
	else if (event->type() == QEvent::MouseMove && !m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool())
	{
		if (m_areaDragCanStart)
		{
			//actually start the drag operation
			if (qApp->mouseButtons() == Qt::LeftButton)
			{
				QList<QWidget*> toolWindows;
				for (int i = 0; i < count(); i++)
				{
					QWidget* toolWindow = widget(i);
					if (qobject_cast<QToolWindowSingleTabAreaFrame*>(toolWindow) == m_pTabFrame)
					{
						toolWindow = m_pTabFrame->contents();
					}
					toolWindows << toolWindow;
				}
				m_areaDragCanStart = false;
				m_manager->startDrag(toolWindows, this);
				releaseMouse();
			}
		}
	}

	return QTabWidget::event(event);
}

QVariantMap QToolWindowArea::saveState()
{
	QVariantMap result;
	result["type"] = "area";
	result["currentIndex"] = currentIndex();
	QStringList objectNames;
	for (int i = 0; i < count(); i++)
	{
		QString name = widget(i)->objectName();
		if (name.isEmpty())
		{
			qWarning("cannot save state of tool window without object name");
		}
		else
		{
			objectNames << name;
		}
	}
	result["objectNames"] = objectNames;
	return result;
}

void QToolWindowArea::restoreState(const QVariantMap& data, int stateFormat)
{
	foreach(QVariant objectNameValue, data["objectNames"].toList())
	{
		QString objectName = objectNameValue.toString();
		if (objectName.isEmpty()) { continue; }
		bool found = false;
		foreach(QWidget * toolWindow, m_manager->toolWindows())
		{
			if (toolWindow->objectName() == objectName)
			{
				addToolWindow(toolWindow);
				found = true;
				break;
			}
		}
		if (!found)
		{
			qWarning("tool window with name '%s' not found", objectName.toLocal8Bit().constData());
		}
	}
	setCurrentIndex(data["currentIndex"].toInt());
}

bool QToolWindowArea::shouldShowSingleTabFrame()
{
	// If we don't allow single tab frames at all
	if (!m_manager->config().value(QTWM_SINGLE_TAB_FRAME, true).toBool())
	{
		return false;
	}
	int c = count();
	// If we have more than one tool window in this area
	if (count() != 1)
	{
		return false;
	}

	// If we are a floating wrapper, it's redundant.
	if (m_manager->isFloatingWrapper(parentWidget()))
	{
		return false;
	}

	return true;
}

void QToolWindowArea::mouseReleaseEvent(QMouseEvent* e)
{
	if (m_manager->config().value(QTWM_SUPPORT_SIMPLE_TOOLS, false).toBool())
	{
		if (e->button() == Qt::RightButton)
		{
			int p = tabBar()->rect().height();

			int tabIndex = tabBar()->tabAt(e->pos());
			if (tabIndex == -1 && e->pos().y() >= 0 && e->pos().y() <= p)
			{
				QAction swap("Swap to Rollups", this);
				e->accept();
				connect(&swap, SIGNAL(triggered()), this, SLOT(swapToRollup()));
				QMenu menu(this);
				menu.addAction(&swap);
				menu.exec(tabBar()->mapToGlobal(QPoint(e->pos().x(), e->pos().y() + 10)));
			}
		}
	}

	QTabWidget::mouseReleaseEvent(e);
}

void QToolWindowArea::adjustDragVisuals()
{
	if (m_manager->config().value(QTWM_SINGLE_TAB_FRAME, true).toBool())
	{
		tabBar()->setAutoHide(true);
		bool showTabFrame = shouldShowSingleTabFrame();

		if (showTabFrame && indexOf(m_pTabFrame) == -1)
		{
			// Enable the single tab frame
			QWidget* w = widget(0);
			removeToolWindow(w);
			m_pTabFrame->setContents(w);
			addToolWindow(m_pTabFrame);
		}
		else if (!showTabFrame && indexOf(m_pTabFrame) != -1)
		{
			// Show the multiple tabs
			QWidget* w = m_pTabFrame->contents();
			m_pTabFrame->setContents(nullptr);
			addToolWindow(w, indexOf(m_pTabFrame));
			removeToolWindow(m_pTabFrame);
		}
	}

	if (!m_manager->isMainWrapper(parentWidget()) && count() > 1 && m_manager->config().value(QTWM_AREA_SHOW_DRAG_HANDLE, false).toBool())
	{
		cornerWidget()->show();
	}

	const bool bMainWidget = m_manager->isMainWrapper(parentWidget());
	if (m_manager->config().value(QTWM_SINGLE_TAB_FRAME, true).toBool() && count() == 1 && m_manager->config().value(QTWM_AREA_TABS_CLOSABLE, false).toBool())
	{
		// Instead of multiple tabs, the single tab frame is visible. So we need to toggle the visibility
		// of its close button. The close button is hidden, when the tab frame is the only widget in
		// its tool window wrapper.
		m_pTabFrame->setCloseButtonVisible(!bMainWidget);
	}
	else if (bMainWidget && count() == 1)
	{
		if (m_useCustomTabCloseButton)
		{
			tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
		}
		else
		{
			setTabsClosable(false);
		}
	}
	else if (m_useCustomTabCloseButton)
	{
		for (int i = 0; i < tabBar()->count(); i++)
		{
			if (!tabBar()->tabButton(i, QTabBar::RightSide))
			{
				tabBar()->setTabButton(i, QTabBar::ButtonPosition::RightSide, createCloseButton());
			}
		}
	}
	else
	{
		setTabsClosable(m_manager->config().value(QTWM_AREA_TABS_CLOSABLE, false).toBool());
	}

	if (m_manager->config().value(QTWM_RETITLE_WRAPPER, true).toBool() && m_manager->isFloatingWrapper(parentWidget()) && count() == 1)
	{
		IToolWindowWrapper* w = wrapper();
		if (w)
		{
			w->getWidget()->setWindowTitle(widget(0)->windowTitle());
		}
	}
	else
	{
		IToolWindowWrapper* w = wrapper();
		if (w)
		{
			w->getWidget()->setWindowTitle(QCoreApplication::applicationName());
		}
	}
}

bool QToolWindowArea::switchAutoHide(bool newValue)
{
	bool result = tabBar()->autoHide();
	tabBar()->setAutoHide(newValue);
	return result;
}

int QToolWindowArea::indexOf(QWidget* w) const
{
	if (w == m_pTabFrame->contents())
	{
		w = m_pTabFrame;
	}
	return QTabWidget::indexOf(w);
}

void QToolWindowArea::setCurrentWidget(QWidget* w)
{
	if (w == m_pTabFrame->contents())
	{
		QTabWidget::setCurrentWidget(m_pTabFrame);
	}
	else
	{
		QTabWidget::setCurrentWidget(w);
	}
}

QRect QToolWindowArea::combineAreaRect() const
{
	if (widget(0) == m_pTabFrame)
		return m_pTabFrame->m_pCaption->rect();
	switch (tabPosition())
	{
	case QTabWidget::West:
	case QTabWidget::East:
		return QRect(0, 0, tabBar()->width(), height());
	case QTabWidget::North:
	case QTabWidget::South:
	default:
		return QRect(0, 0, width(), tabBar()->height());
	}
}

void QToolWindowArea::showContextMenu(const QPoint& point)
{
	//If this is the only window on the main wrapper (or the last tab) we need to keep it there
	if (point.isNull() || (m_manager->isMainWrapper(parentWidget()) && (tabBar()->count() <= 1)))
		return;

	QToolWindowSingleTabAreaFrame* singleTabFrame = qobject_cast<QToolWindowSingleTabAreaFrame*>(currentWidget());

	//Check if the tab frame is the only widget in its tool window wrapper and don't show the context menu in that case
	if (singleTabFrame && singleTabFrame->m_pCaption && singleTabFrame->m_pCaption->contentsRect().contains(point) && !m_manager->isMainWrapper(parentWidget()))
	{
		QMenu menu(this);
		connect(menu.addAction(tr("Close")), &QAction::triggered, [this, singleTabFrame]() { singleTabFrame->close(); });

		menu.exec(mapToGlobal(QPoint(point.x(), point.y())));
		return;
	}

	int tabIndex = tabBar()->tabAt(point);
	if (tabIndex >= 0)
	{
		widget(tabIndex)->customContextMenuRequested(tabBar()->mapToGlobal(point));
	}
}

void QToolWindowArea::swapToRollup()
{
	m_manager->SwapAreaType(this, watRollups);
}

QToolWindowSingleTabAreaFrame::QToolWindowSingleTabAreaFrame(QToolWindowManager* manager, QWidget* parent)
	: QFrame(parent)
	, m_pLayout(new QGridLayout(this))
	, m_manager(manager)
	, m_closeButton(nullptr)
	, m_pCaption(new QLabel(this))
	, m_pContents(nullptr)
{
	m_pLayout->setContentsMargins(0, 0, 0, 0);
	m_pLayout->setMargin(0);
	m_pLayout->setSpacing(0);

	m_pCaption->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_pCaption->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_pLayout->addWidget(m_pCaption, 0, 0);
	if (m_manager->config().value(QTWM_AREA_TABS_CLOSABLE, false).toBool())
	{
		m_closeButton = new QPushButton(this);
		m_closeButton->setObjectName("closeButton");
		m_closeButton->setFocusPolicy(Qt::NoFocus);
		m_closeButton->setIcon(getCloseButtonIcon());
		connect(m_closeButton, SIGNAL(clicked()), this, SLOT(closeWidget()));
		m_pLayout->addWidget(m_closeButton, 0, 2);
	}
	else
	{
		m_pLayout->addItem(new QSpacerItem(0, 23, QSizePolicy::Minimum, QSizePolicy::Expanding), 0, 1);
	}
	connect(this, SIGNAL(windowTitleChanged(const QString&)), m_pCaption, SLOT(setText(const QString&)));
	m_pLayout->setColumnStretch(0, 1);
	m_pLayout->setRowStretch(1, 1);
}

QIcon QToolWindowSingleTabAreaFrame::getCloseButtonIcon() const
{
	return getIcon(m_manager->config(), QTWM_SINGLE_TAB_FRAME_CLOSE_ICON, QIcon(":/QtDockLibrary/gfx/close.png"));
}

void QToolWindowSingleTabAreaFrame::setContents(QWidget* widget)
{
	if (m_pContents)
	{
		m_pLayout->removeWidget(m_pContents);
	}
	if (widget)
	{
		m_pLayout->addWidget(widget, 1, 0, 1, 2);
		widget->show();
		setObjectName(widget->objectName());
		setWindowIcon(widget->windowIcon());
		setWindowTitle(widget->windowTitle());
		QObject::connect(widget, &QWidget::windowTitleChanged, this, &QWidget::setWindowTitle);
		QObject::connect(widget, &QWidget::windowIconChanged, this, &QWidget::setWindowIcon);
	}
	m_pContents = widget;
}

void QToolWindowSingleTabAreaFrame::setCloseButtonVisible(bool bVisible)
{
	// Instead of actually hiding the close button, we set an empty icon and disable signals.
	// This way, the containing layout will retain its size, and the button will still draw its
	// background.
	// Setting the buttons visibility to false, on the other hand, would cause a black square hole
	// in the layout.
	if (bVisible)
	{
		m_closeButton->setIcon(getCloseButtonIcon());
		m_closeButton->blockSignals(false);
	}
	else
	{
		m_closeButton->setIcon(QIcon());
		m_closeButton->blockSignals(true);
	}
}

void QToolWindowSingleTabAreaFrame::closeWidget()
{
	m_manager->releaseToolWindow(m_pContents, true);
}

void QToolWindowSingleTabAreaFrame::closeEvent(QCloseEvent* e)
{
	if (m_pContents)
	{
		e->setAccepted(m_pContents->close());
		if (e->isAccepted())
		{
			closeWidget();
		}
	}
}
