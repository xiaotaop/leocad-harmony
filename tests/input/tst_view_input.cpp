#include "lc_global.h"
#include <QtTest>
#include "lc_application.h"
#include "lc_context.h"
#include "lc_thumbnailmanager.h"
#include <QScopeGuard>
#include <QSemaphore>
#include <QThreadPool>
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_model.h"
#include "lc_partselectionwidget.h"
#include "lc_view.h"
#include "lc_viewwidget.h"
#include "lc_viewsphere.h"
#include "lc_shortcuts.h"
#include "camera.h"
#include "piece.h"
#include "pieceinf.h"
#include "project.h"

class ViewInputTests : public QObject
{
    Q_OBJECT
    std::unique_ptr<lcMainWindow> Window;
    lcView* View = nullptr;
    lcViewWidget* Widget = nullptr;
    lcModel* Model = nullptr;
    void mouse(QEvent::Type Type, QPointF Position, Qt::MouseButton Button, Qt::MouseButtons Buttons, Qt::KeyboardModifiers Modifiers=Qt::NoModifier)
    {
        QMouseEvent Event(Type,Position,Widget->mapToGlobal(Position.toPoint()),Button,Buttons,Modifiers);
        QApplication::sendEvent(Widget,&Event);
    }
    void wheel(Qt::ScrollPhase Phase,QPoint Delta)
    {
        const QPointF P=center();
        QWheelEvent Event(P,Widget->mapToGlobal(P.toPoint()),Delta,QPoint(),Qt::NoButton,Qt::NoModifier,Phase,false,Qt::MouseEventSynthesizedBySystem);
        QApplication::sendEvent(Widget,&Event);
    }
    void capture(const QString& Name)
    {
        const QString Directory=qEnvironmentVariable("LEOCAD_INPUT_EVIDENCE_DIR");
        if(Directory.isEmpty()) return;
        QVERIFY(QDir().mkpath(Directory));
        QVERIFY(Widget->grabFramebuffer().save(Directory+"/"+Name+".png"));
    }
    QPointF fromView(const lcVector3& P) const
    {
        const float Scale=Widget->GetDeviceScale();
        return QPointF(P[0]/Scale,(View->GetHeight()-1-P[1])/Scale);
    }
    QPointF center() const { return QPointF(Widget->width()/2.0,Widget->height()/2.0); }
    bool openAndRefresh(const QString& Path)
    {
        if(!Window->OpenProject(Path)) return false;
        View=Window->GetActiveView(); Widget=View?View->GetWidget():nullptr;
        Model=lcGetActiveProject()->GetActiveModel();
        if(!View || !Widget || !Model) return false;
        QApplication::setActiveWindow(Window.get()); Widget->setFocus();
        QCoreApplication::processEvents();
        return true;
    }
private slots:
    void rayBoxBoundaries_data()
    {
        QTest::addColumn<lcVector3>("Start"); QTest::addColumn<lcVector3>("End"); QTest::addColumn<bool>("Hit");
        QTest::newRow("captured-dpr1-edge") << lcVector3(54.29780197143555f,-54.29780197143555f,17.148897171020508f) << lcVector3(-33260.55859375f,33260.55859375f,-16640.279296875f) << true;
        QTest::newRow("edge") << lcVector3(50,-50,0) << lcVector3(0,0,0) << true;
        QTest::newRow("corner") << lcVector3(50,-50,34) << lcVector3(0,0,-16) << true;
        QTest::newRow("inside") << lcVector3(0,0,0) << lcVector3(10,10,10) << true;
        QTest::newRow("parallel-miss") << lcVector3(21,0,50) << lcVector3(21,0,-50) << false;
        QTest::newRow("near-edge-miss") << lcVector3(50,20.001f,0) << lcVector3(0,20.001f,0) << false;
        QTest::newRow("away") << lcVector3(50,0,0) << lcVector3(100,0,0) << false;
        QTest::newRow("zero-direction") << lcVector3(50,0,0) << lcVector3(50,0,0) << false;
        QTest::newRow("forward-ray-beyond-end") << lcVector3(50,0,0) << lcVector3(40,0,0) << true;
    }
    void rayBoxBoundaries()
    {
        QFETCH(lcVector3,Start); QFETCH(lcVector3,End); QFETCH(bool,Hit);
        const lcVector3 Min(-20,-20,-24), Max(20,20,4);
        float Distance=FLT_MAX; lcVector3 Point, Plane;
        QCOMPARE(lcBoundingBoxRayIntersectDistance(Min,Max,Start,End,&Distance,&Point,&Plane),Hit);
        if(Hit)
        {
            QVERIFY(std::isfinite(Distance) && Distance>=0);
            for(int Axis=0;Axis<3;Axis++) QVERIFY(Point[Axis]>=Min[Axis] && Point[Axis]<=Max[Axis]);
        }
    }
    void init()
    {
        if(qstrcmp(QTest::currentTestFunction(),"rayBoxBoundaries")==0) return;
        auto& Preferences=lcGetPreferences();
        Preferences.mViewSphereEnabled=true; Preferences.mViewSphereSize=100;
        Preferences.mViewSphereLocation=lcViewSphereLocation::TopRight;
        Window=std::make_unique<lcMainWindow>(); Window->CreateWidgets();
        Window->SetColorIndex(lcGetColorIndex(4));
        // Input tests own the canvas. Parts-panel thumbnail lifecycle is
        // covered by its separate suite, not by repeated window fixtures.
        for(QDockWidget* Dock : Window->findChildren<QDockWidget*>()) Dock->hide();
        gApplication->SetProject(new Project());
        Model=lcGetActiveProject()->GetActiveModel();
        PieceInfo* Info=lcGetPiecesLibrary()->FindPiece("3003.dat",lcGetActiveProject(),false,false);
        QVERIFY(Info); QVERIFY(Model->AddPiece(Info,LC_PIECE_SECTION_INVALID));
        Model->ClearSelectionAction(); Model->SetSaved();
        Window->resize(1100,800); Window->show();
        QVERIFY(QTest::qWaitForWindowExposed(Window.get()));
        View=Window->GetActiveView(); QVERIFY(View); Widget=View->GetWidget(); QVERIFY(Widget); QTRY_VERIFY(Widget->isValid());
        Widget->setFocus(); View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents();
        qInfo()<<"INPUT_WIDGET"<<Widget->size()<<"DPR"<<Widget->GetDeviceScale()<<"view"<<View->GetWidth()<<View->GetHeight();
    }
    void cleanup()
    {
        Window.reset(); delete gApplication->mProject; gApplication->mProject=nullptr;
        View=nullptr; Widget=nullptr; Model=nullptr;
    }
    void navigation_data()
    {
        QTest::addColumn<int>("Tool");
        QTest::newRow("orbit")<<int(lcTool::RotateView);
        QTest::newRow("pan")<<int(lcTool::Pan);
        QTest::newRow("zoom")<<int(lcTool::Zoom);
    }
    void navigation()
    {
        QFETCH(int,Tool); Window->SetTool(lcTool(Tool));
        const lcVector3 Before=View->GetCamera()->mPosition;
        const QPointF P=center(); mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton); QVERIFY(View->IsTracking());
        mouse(QEvent::MouseMove,P+QPointF(40,25),Qt::NoButton,Qt::LeftButton);
        QVERIFY(View->GetCamera()->mPosition!=Before);
        mouse(QEvent::MouseButtonRelease,P+QPointF(40,25),Qt::LeftButton,Qt::NoButton);
        QVERIFY(!View->IsTracking()); QVERIFY(!Model->IsModified());
        capture(QString("navigation-%1").arg(Tool));
    }
    void focusLossStopsTracking_data()
    {
        QTest::addColumn<int>("EventType");
        QTest::newRow("focus")<<int(QEvent::FocusOut);
        QTest::newRow("deactivate")<<int(QEvent::WindowDeactivate);
        QTest::newRow("hide")<<int(QEvent::Hide);
        QTest::newRow("ungrab")<<int(QEvent::UngrabMouse);
    }
    void focusLossStopsTracking()
    {
        QFETCH(int,EventType);
        lcPiece* Piece=Model->GetPieces()[0].get();
        Model->SetSelectionAndFocusAction({Piece},Piece,LC_PIECE_SECTION_POSITION,lcSelectionMode::Single);
        Window->SetTool(lcTool::RotateView); const QPointF P=center();
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton); QVERIFY(View->IsTracking());
        mouse(QEvent::MouseMove,P+QPointF(40,25),Qt::NoButton,Qt::LeftButton);
        if(EventType==int(QEvent::FocusOut))
        {
            QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        }
        else
        {
            QEvent Lost{QEvent::Type(EventType)}; QApplication::sendEvent(Widget,&Lost);
        }
        QVERIFY(!View->IsTracking());
        QVERIFY(Model->GetPieces()[0]->IsSelected());
        const lcVector3 After=View->GetCamera()->mPosition;
        mouse(QEvent::MouseMove,P+QPointF(80,50),Qt::NoButton,Qt::NoButton);
        QCOMPARE(View->GetCamera()->mPosition,After);
        mouse(QEvent::MouseButtonRelease,P,Qt::LeftButton,Qt::NoButton);
        QCOMPARE(View->GetCamera()->mPosition,After);
        QFocusEvent LostAgain(QEvent::FocusOut); QApplication::sendEvent(Widget,&LostAgain);
        // Recovery must not depend on a hover/move before the fresh press.
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton);
        QVERIFY(View->IsTracking());
        mouse(QEvent::MouseMove,P+QPointF(25,10),Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,P+QPointF(25,10),Qt::LeftButton,Qt::NoButton);
        QVERIFY(!View->IsTracking());
    }
    void defaultMouseShortcuts_data()
    {
        QTest::addColumn<int>("Modifiers");
        QTest::newRow("right-orbit")<<int(Qt::NoModifier);
        QTest::newRow("shift-right-pan")<<int(Qt::ShiftModifier);
        QTest::newRow("alt-right-zoom")<<int(Qt::AltModifier);
    }
    void defaultMouseShortcuts()
    {
        QFETCH(int,Modifiers); Window->SetTool(lcTool::Select);
        const QPointF P=center(); const lcVector3 Before=View->GetCamera()->mPosition;
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton,Qt::KeyboardModifiers(Modifiers));
        mouse(QEvent::MouseButtonPress,P,Qt::RightButton,Qt::RightButton,Qt::KeyboardModifiers(Modifiers));
        QVERIFY(View->IsTracking());
        mouse(QEvent::MouseMove,P+QPointF(45,30),Qt::NoButton,Qt::RightButton,Qt::KeyboardModifiers(Modifiers));
        mouse(QEvent::MouseButtonRelease,P+QPointF(45,30),Qt::RightButton,Qt::NoButton,Qt::KeyboardModifiers(Modifiers));
        QVERIFY(!View->IsTracking()); QVERIFY(View->GetCamera()->mPosition!=Before);
        QVERIFY(!Model->IsModified());
    }
    void pickingAcrossResize()
    {
        for(QSize Size : {QSize(1100,800),QSize(900,700),QSize(1300,850),QSize(1100,800)})
        {
            Window->resize(Size); QCoreApplication::processEvents();
            Window->SetTool(lcTool::Select); View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents();
            QCOMPARE(View->GetWidth(),int(Widget->width()*Widget->GetDeviceScale()));
            QCOMPARE(View->GetHeight(),int(Widget->height()*Widget->GetDeviceScale()));
            lcPiece* Piece=Model->GetPieces()[0].get();
            const auto& Bounds=Piece->mPieceInfo->GetBoundingBox();
            const lcVector3 Position=lcMul31((Bounds.Min+Bounds.Max)/2.0f,Piece->mModelWorld);
            const QPointF P=fromView(View->ProjectPoint(Position));
            Model->ClearSelectionAction();
            mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
            const lcMatrix44 Inverse=lcMatrix44AffineInverse(Piece->mModelWorld);
            const lcVector3 Start=lcMul31(View->UnprojectPoint(lcVector3(View->GetMouseX(),View->GetMouseY(),0)),Inverse);
            const lcVector3 End=lcMul31(View->UnprojectPoint(lcVector3(View->GetMouseX(),View->GetMouseY(),1)),Inverse);
            float Distance=FLT_MAX;
            const bool BoxHit=lcBoundingBoxRayIntersectDistance(Bounds.Min,Bounds.Max,Start,End,&Distance,nullptr,nullptr);
            qInfo()<<"PICK"<<Size<<P<<"view xy"<<View->GetMouseX()<<View->GetMouseY()<<"ray hit"<<View->FindObjectUnderPointer(true,false).Object<<"box"<<BoxHit;
            qInfo().noquote()<<"LOCAL_RAY"<<QJsonDocument(QJsonObject{{"start",QJsonArray{double(Start[0]),double(Start[1]),double(Start[2])}},{"end",QJsonArray{double(End[0]),double(End[1]),double(End[2])}}}).toJson(QJsonDocument::Compact);
            capture(QString("pick-%1-%2").arg(Size.width()).arg(Size.height()));
            mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease,P,Qt::LeftButton,Qt::NoButton);
            QCOMPARE(Model->GetFocusObject(),static_cast<lcObject*>(Piece));
            QVERIFY(Piece->IsSelected()); QVERIFY(!View->IsTracking());
        }
        capture("picked-resized-restored");
    }
    void lostButtonStateStopsTracking()
    {
        Window->SetTool(lcTool::Pan); const QPointF P=center();
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton); QVERIFY(View->IsTracking());
        mouse(QEvent::MouseMove,P+QPointF(25,10),Qt::NoButton,Qt::LeftButton);
        const lcVector3 Before=View->GetCamera()->mPosition;
        mouse(QEvent::MouseMove,P+QPointF(50,20),Qt::NoButton,Qt::NoButton);
        QVERIFY(!View->IsTracking()); QCOMPARE(View->GetCamera()->mPosition,Before);
    }
    void moveCancellationRollsBackAndFreshMoveCommits()
    {
        lcPiece* Piece=Model->GetPieces()[0].get();
        Model->SetSelectionAndFocusAction({Piece},Piece,LC_PIECE_SECTION_POSITION,lcSelectionMode::Single);
        Window->SetMoveSnapEnabled(false); Window->SetTool(lcTool::Move); Model->SetSaved();
        const lcVector3 Start=Piece->mModelWorld.GetTranslation(); const QPointF P=center();
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton); QVERIFY(View->IsTracking());
        mouse(QEvent::MouseMove,P+QPointF(70,35),Qt::NoButton,Qt::LeftButton);
        QVERIFY(Model->GetPieces()[0]->mModelWorld.GetTranslation()!=Start);
        QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        QVERIFY(!View->IsTracking());
        QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Start);
        QVERIFY(!Model->IsModified());
        QVERIFY(Model->GetPieces()[0]->IsSelected());
        mouse(QEvent::MouseButtonRelease,P+QPointF(70,35),Qt::LeftButton,Qt::NoButton);
        QVERIFY(!Model->IsModified());
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,P+QPointF(70,35),Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,P+QPointF(70,35),Qt::LeftButton,Qt::NoButton);
        QVERIFY(Model->IsModified()); QVERIFY(Model->GetPieces()[0]->mModelWorld.GetTranslation()!=Start);
        capture("fresh-move-after-cancel");
        Model->UndoAction(); QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Start); QVERIFY(!Model->IsModified());
    }
    void windowEventDragCommits()
    {
        lcPiece* Piece=Model->GetPieces()[0].get();
        Model->SetSelectionAndFocusAction({Piece},Piece,LC_PIECE_SECTION_POSITION,lcSelectionMode::Single);
        Window->SetMoveSnapEnabled(false); Window->SetTool(lcTool::Move); Model->SetSaved();
        const lcVector3 Start=Piece->mModelWorld.GetTranslation();
        const QPoint P=Widget->mapTo(Window.get(),center().toPoint()), End=P+QPoint(70,35);
        // QWindow delivery traverses QWidgetWindow's implicit mouse capture
        // path as well as our widget handlers. Ordinary releases must commit.
        QTest::mouseMove(Window->windowHandle(),P);
        QTest::mousePress(Window->windowHandle(),Qt::LeftButton,Qt::NoModifier,P);
        QVERIFY(View->IsTracking());
        QTest::mouseMove(Window->windowHandle(),End,10);
        QVERIFY(Model->GetPieces()[0]->mModelWorld.GetTranslation()!=Start);
        QTest::mouseRelease(Window->windowHandle(),Qt::LeftButton,Qt::NoModifier,End);
        QVERIFY(!View->IsTracking()); QVERIFY(Model->IsModified());
        Model->UndoAction(); QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Start); QVERIFY(!Model->IsModified());
    }
    void cancelledZoomRegionDoesNotZoom()
    {
        Window->SetTool(lcTool::ZoomRegion); const QPointF P=center()-QPointF(60,40);
        const lcVector3 Before=View->GetCamera()->mPosition;
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,P+QPointF(120,80),Qt::NoButton,Qt::LeftButton);
        QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        QVERIFY(!View->IsTracking()); QCOMPARE(View->GetCamera()->mPosition,Before);
        mouse(QEvent::MouseButtonRelease,P+QPointF(120,80),Qt::LeftButton,Qt::NoButton);
        QCOMPARE(View->GetCamera()->mPosition,Before);
    }
    void orphanPanUpdatesAreIgnoredAfterFocusLoss()
    {
        const lcVector3 Before=View->GetCamera()->mPosition;
        wheel(Qt::ScrollBegin,QPoint()); wheel(Qt::ScrollUpdate,QPoint(20,10));
        QVERIFY(View->GetCamera()->mPosition!=Before);
        QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        const lcVector3 After=View->GetCamera()->mPosition;
        wheel(Qt::ScrollUpdate,QPoint(50,25)); wheel(Qt::ScrollEnd,QPoint());
        QCOMPARE(View->GetCamera()->mPosition,After);
        wheel(Qt::ScrollBegin,QPoint()); wheel(Qt::ScrollUpdate,QPoint(20,10)); wheel(Qt::ScrollEnd,QPoint());
        QVERIFY(View->GetCamera()->mPosition!=After); QVERIFY(!Model->IsModified());
    }
    void viewSphereDoesNotResumeAfterFocusLoss()
    {
        const float HalfSize=lcGetPreferences().mViewSphereSize*View->GetUIScale()/2.0f;
        const QPointF P=fromView(lcVector3(View->GetWidth()-HalfSize,View->GetHeight()-HalfSize,0));
        mouse(QEvent::MouseMove,P,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton);
        QVERIFY(!View->IsTracking()); // Sphere starts tracking only after movement.
        QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        const lcVector3 Before=View->GetCamera()->mPosition;
        mouse(QEvent::MouseMove,P+QPointF(15,10),Qt::NoButton,Qt::NoButton);
        QVERIFY(!View->IsTracking());
        mouse(QEvent::MouseButtonRelease,P+QPointF(15,10),Qt::LeftButton,Qt::NoButton);
        QCOMPARE(View->GetCamera()->mPosition,Before);
    }
public:
    void angleWheel(int Delta, Qt::KeyboardModifiers Modifiers=Qt::NoModifier, bool Inverted=false)
    {
        const QPointF P=center();
        QWheelEvent Event(P,Widget->mapToGlobal(P.toPoint()),QPoint(),QPoint(0,Delta),Qt::NoButton,Modifiers,Qt::NoScrollPhase,Inverted);
        QApplication::sendEvent(Widget,&Event);
        QVERIFY(Event.isAccepted());
    }
private slots:
    void highResolutionWheel_data()
    {
        QTest::addColumn<int>("Delta");
        QTest::newRow("positive-30")<<30;
        QTest::newRow("negative-30")<<-30;
        QTest::newRow("positive-8")<<8;
        QTest::newRow("negative-8")<<-8;
    }
    void highResolutionWheel()
    {
        QFETCH(int,Delta);
        const lcVector3 Before=View->GetCamera()->mPosition;
        const int Count=120/std::abs(Delta);
        for(int i=1;i<Count;i++) {angleWheel(Delta);QCOMPARE(View->GetCamera()->mPosition,Before);}
        angleWheel(Delta); const lcVector3 Accumulated=View->GetCamera()->mPosition;
        QVERIFY(Accumulated!=Before);
        View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents();
        QCOMPARE(View->GetCamera()->mPosition,Before);
        angleWheel(Delta>0?120:-120);
        QCOMPARE(View->GetCamera()->mPosition,Accumulated);
        QVERIFY(!Model->IsModified());
    }
    void wheelRemainderAndFocusReset()
    {
        const lcVector3 Before=View->GetCamera()->mPosition;
        angleWheel(150); const lcVector3 First=View->GetCamera()->mPosition; QVERIFY(First!=Before);
        angleWheel(60); QCOMPARE(View->GetCamera()->mPosition,First);
        angleWheel(30); QVERIFY(View->GetCamera()->mPosition!=First);
        const lcVector3 Second=View->GetCamera()->mPosition;
        angleWheel(90); QCOMPARE(View->GetCamera()->mPosition,Second);
        QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        angleWheel(30); QCOMPARE(View->GetCamera()->mPosition,Second);
        angleWheel(-30); QCOMPARE(View->GetCamera()->mPosition,Second);
        angleWheel(120); QVERIFY(View->GetCamera()->mPosition!=Second);
        QVERIFY(!Model->IsModified());
    }
    void wheelModifiersAndHorizontalInput()
    {
        const lcVector3 Before=View->GetCamera()->mPosition;
        auto Distance=[&]() {return lcLength(View->GetCamera()->mPosition-Before);};
        const float Original=0;
        angleWheel(120,Qt::ShiftModifier); const float Fine=std::abs(Distance()-Original);
        View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents();
        angleWheel(120); const float Normal=std::abs(Distance()-Original);
        View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents();
        angleWheel(120,Qt::ControlModifier); const float Fast=std::abs(Distance()-Original);
        QVERIFY(Fine>0 && Fine<Normal && Normal<Fast);
        View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents();
        angleWheel(120,Qt::NoModifier,true); // angleDelta already carries the platform's direction.
        const lcVector3 Inverted=View->GetCamera()->mPosition;
        View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents(); angleWheel(120);
        QCOMPARE(View->GetCamera()->mPosition,Inverted);
        const lcVector3 After=View->GetCamera()->mPosition;
        const QPointF P=center();
        QWheelEvent Horizontal(P,Widget->mapToGlobal(P.toPoint()),QPoint(),QPoint(120,0),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(Widget,&Horizontal);
        QVERIFY(!Horizontal.isAccepted()); QCOMPARE(View->GetCamera()->mPosition,After);
        QVERIFY(Before!=After); QVERIFY(!Model->IsModified());
    }
    void keyboardMoveUndoRedo()
    {
        lcPiece* Piece=Model->GetPieces()[0].get();
        Model->SetSelectionAndFocusAction({Piece},Piece,LC_PIECE_SECTION_POSITION,lcSelectionMode::Single);
        Model->SetSaved(); const lcVector3 Before=Piece->mModelWorld.GetTranslation();
        // Establish Qt shortcut context; desktop foreground policy is not
        // part of this injected-event test (physical focus is a device check).
        QApplication::setActiveWindow(Window.get()); Widget->setFocus();
        QVERIFY(Window->isActiveWindow()); QVERIFY(Widget->hasFocus());
        QTest::keyClick(Widget,Qt::Key_Right);
        const lcVector3 After=Model->GetPieces()[0]->mModelWorld.GetTranslation();
        QVERIFY(After!=Before); QVERIFY(Model->IsModified());
        QTest::keySequence(Widget,QKeySequence("Ctrl+Z"));
        QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Before); QVERIFY(!Model->IsModified());
        QTest::keySequence(Widget,QKeySequence("Ctrl+Y"));
        QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),After); QVERIFY(Model->IsModified());
    }
    void modifierDragSemantics_data()
    {
        QTest::addColumn<int>("Modifiers");
        QTest::newRow("shift-pan")<<int(Qt::ShiftModifier);
        QTest::newRow("alt-zoom")<<int(Qt::AltModifier);
    }
    void modifierDragSemantics()
    {
        QFETCH(int,Modifiers); Window->SetTool(lcTool::Select);
        const lcVector3 Before=View->GetCamera()->mPosition, Target=View->GetCamera()->mTargetPosition;
        const QPointF P=center(); const auto Mods=Qt::KeyboardModifiers(Modifiers);
        QTest::keyPress(Widget,Modifiers==int(Qt::ShiftModifier)?Qt::Key_Shift:Qt::Key_Alt);
        mouse(QEvent::MouseButtonPress,P,Qt::RightButton,Qt::RightButton,Mods);
        mouse(QEvent::MouseMove,P+QPointF(35,25),Qt::NoButton,Qt::RightButton,Mods);
        mouse(QEvent::MouseButtonRelease,P+QPointF(35,25),Qt::RightButton,Qt::NoButton,Mods);
        QTest::keyRelease(Widget,Modifiers==int(Qt::ShiftModifier)?Qt::Key_Shift:Qt::Key_Alt);
        const lcVector3 After=View->GetCamera()->mPosition;
        QVERIFY(After!=Before);
        if(Modifiers==int(Qt::ShiftModifier))
        {
            QVERIFY(lcLength((After-Before)-(View->GetCamera()->mTargetPosition-Target))<0.001f);
        }
        else
        {
            QVERIFY(lcLength((After-Before)-(View->GetCamera()->mTargetPosition-Target))<0.001f);
            QVERIFY(lcLength(lcCross(lcNormalize(Before-Target),lcNormalize(After-Before)))<0.001f);
        }
        QVERIFY(!View->IsTracking()); QVERIFY(!Model->IsModified());
    }
    void thumbnailSharedRequestLifetime()
    {
        auto* Manager=lcGetPiecesLibrary()->GetThumbnailManager();
        PieceInfo* Info=Model->GetPieces()[0]->mPieceInfo;
        std::vector<lcPartThumbnailId> Owned;
        auto Cleanup=qScopeGuard([&]() {for(auto Id:Owned) Manager->ReleaseThumbnail(Id);});
        auto First=Manager->RequestThumbnail(Info,lcGetColorIndex(4),73,1);
        Owned.push_back(First.first); QVERIFY(!First.second.isNull());
        auto Second=Manager->RequestThumbnail(Info,lcGetColorIndex(4),73,1);
        Owned.push_back(Second.first); QCOMPARE(Second.first,First.first);
        Manager->ReleaseThumbnail(First.first); Owned.erase(Owned.begin());
        auto Third=Manager->RequestThumbnail(Info,lcGetColorIndex(4),73,1);
        Owned.push_back(Third.first);
        QCOMPARE(Third.first,Second.first); QVERIFY(!Third.second.isNull());
    }
    void thumbnailDefersStaleCompletion()
    {
        auto* Library=lcGetPiecesLibrary(); auto* Manager=Library->GetThumbnailManager();
        PieceInfo* Info=Model->GetPieces()[0]->mPieceInfo;
        const int Before=Info->GetRefCount();
        // Model owns a fully loaded mesh. Delay publication of completion on
        // the GUI loop to reproduce a stale notification during a new load.
        Info->mState=lcPieceInfoState::Loading;
        auto Restore=qScopeGuard([&]() {Info->mState=lcPieceInfoState::Loaded;});
        auto Pending=Manager->RequestThumbnail(Info,lcGetColorIndex(1),79,1);
        auto Release=qScopeGuard([&]() {Manager->ReleaseThumbnail(Pending.first);});
        QVERIFY(Pending.second.isNull()); QCOMPARE(Info->GetRefCount(),Before+1);
        QSignalSpy Ready(Manager,&lcThumbnailManager::ThumbnailReady);
        QTimer Complete; Complete.setSingleShot(true);
        connect(&Complete,&QTimer::timeout,this,[&]() {
            Info->mState=lcPieceInfoState::Loaded;
            Library->PartLoaded(Info);
        });
        Complete.start(0);
        qInfo()<<"D9_STALE_CALLBACK before dispatch";
        QVERIFY(QMetaObject::invokeMethod(Manager,"PartLoaded",Qt::DirectConnection,Q_ARG(PieceInfo*,Info)));
        QCOMPARE(Ready.count(),0); QCOMPARE(Info->GetRefCount(),Before+1);
        QTRY_COMPARE(Ready.count(),1);
        QCOMPARE(qvariant_cast<lcPartThumbnailId>(Ready.at(0).at(0)),Pending.first);
        QVERIFY(!qvariant_cast<QPixmap>(Ready.at(0).at(1)).isNull());
        QCOMPARE(Info->mState,lcPieceInfoState::Loaded);
        qInfo()<<"D9_STALE_CALLBACK completed";
    }
    void thumbnailCancelledLoadDoesNotNotify()
    {
        auto* Library=lcGetPiecesLibrary();
        auto* Manager=Library->GetThumbnailManager();
        PieceInfo* Info=Library->FindPiece("3001.dat",lcGetActiveProject(),false,false);
        QVERIFY(Info);
        QCOMPARE(Info->GetRefCount(),0);

        QSignalSpy Completed(Library,&lcPiecesLibrary::PartLoaded);
        QSignalSpy Ready(Manager,&lcThumbnailManager::ThumbnailReady);
        const auto Cancelled=Manager->RequestThumbnail(Info,lcGetColorIndex(4),67,1);
        Library->WaitForLoadQueue();
        Manager->ReleaseThumbnail(Cancelled.first);
        QCOMPARE(Info->mState,lcPieceInfoState::Unloaded);

        const auto Current=Manager->RequestThumbnail(Info,lcGetColorIndex(1),67,1);
        auto Release=qScopeGuard([&]() {Manager->ReleaseThumbnail(Current.first);});
        Library->WaitForLoadQueue();
        QCoreApplication::sendPostedEvents();
        QCOMPARE(Completed.count(),1);
        QCOMPARE(Ready.count(),1);
        QCOMPARE(qvariant_cast<lcPartThumbnailId>(Ready.at(0).at(0)),Current.first);
        QVERIFY(!qvariant_cast<QPixmap>(Ready.at(0).at(1)).isNull());
    }
    void thumbnailQueuedTemporaryCanBeRemoved()
    {
        auto* Library=lcGetPiecesLibrary();
        auto* Manager=Library->GetThumbnailManager();
        Library->WaitForLoadQueue();
        QCoreApplication::sendPostedEvents();

        auto* Pool=QThreadPool::globalInstance();
        const int ThreadCount=Pool->maxThreadCount();
        QSemaphore Entered,Unblock,Exited;
        Pool->setMaxThreadCount(1);
        Pool->start([&]() {Entered.release(); Unblock.acquire(); Exited.release();});
        auto Restore=qScopeGuard([&]() {
            Unblock.release();
            Exited.acquire();
            Library->WaitForLoadQueue();
            Pool->setMaxThreadCount(ThreadCount);
        });
        QVERIFY(Entered.tryAcquire(1,5000));

        constexpr char Name[]="d10-unlisted-cancelled.dat";
        auto* Info=Library->FindPiece(Name,lcGetActiveProject(),true,false);
        QVERIFY(Info && Info->IsPlaceholder());
        QSignalSpy Completed(Library,&lcPiecesLibrary::PartLoaded);
        const auto Pending=Manager->RequestThumbnail(Info,lcGetColorIndex(4),69,1);
        Manager->ReleaseThumbnail(Pending.first);
        Library->RemoveTemporaryPieces();
        QVERIFY(!Library->FindPiece(Name,lcGetActiveProject(),false,false));
        Restore.dismiss();
        Unblock.release();
        Exited.acquire();
        Library->WaitForLoadQueue();
        Pool->setMaxThreadCount(ThreadCount);
        QCoreApplication::sendPostedEvents();
        QCOMPARE(Completed.count(),0);
    }
    void renderScale_data()
    {
        QTest::addColumn<int>("Count");
        QTest::newRow("500")<<500;
        QTest::newRow("2000")<<2000;
        QTest::newRow("5000")<<5000;
    }
    void renderScale()
    {
        QFETCH(int,Count);
        QTemporaryDir Files;
        QVERIFY(Files.isValid());
        const QString Name=QStringLiteral("d10-%1.mpd").arg(Count);
        const QString Path=Files.filePath(Name);
        static constexpr std::array<int,16> Parts{
            3001,3002,3003,3004,3005,3008,3010,3020,
            3021,3022,3024,3030,3031,3032,3622,3039
        };
        QFile File(Path);
        QVERIFY(File.open(QIODevice::WriteOnly));
        QByteArray Mpd("0 FILE main.ldr\n");
        for(int Child=0;Child<4;Child++)
            Mpd+=QStringLiteral("1 16 %1 0 0 1 0 0 0 1 0 0 0 1 child-%2.ldr\n").arg(Child*1040).arg(Child).toLatin1();
        Mpd+="0 NOFILE\n";
        for(int Child=0;Child<4;Child++)
        {
            Mpd+=QStringLiteral("0 FILE child-%1.ldr\n").arg(Child).toLatin1();
            for(int Index=0;Index<Count/4;Index++)
            {
                const int Color=1+(Index+Child)%15;
                Mpd+=QStringLiteral("1 %1 %2 %3 %4 1 0 0 0 1 0 0 0 1 %5.dat\n")
                    .arg(Color).arg((Index%20)*50).arg(-(Index/400)*32)
                    .arg(((Index/20)%20)*50).arg(Parts[(Index+Child)%Parts.size()]).toLatin1();
            }
            Mpd+="0 NOFILE\n";
        }
        QCOMPARE(File.write(Mpd),qint64(Mpd.size()));
        File.close();
        const QString Export=qEnvironmentVariable("LEOCAD_D10_FIXTURE_DIR");
        if(!Export.isEmpty())
        {
            QVERIFY(QDir().mkpath(Export));
            QFile Copy(QDir(Export).filePath(Name));
            QVERIFY(Copy.open(QIODevice::WriteOnly));
            QCOMPARE(Copy.write(Mpd),qint64(Mpd.size()));
        }

        for(const char* Stage : {"open","reopen"})
        {
            qInfo()<<"D10_RENDER_STAGE"<<Count<<Stage;
            QElapsedTimer Timer;
            Timer.start();
            QVERIFY(Window->OpenProject(Path));
            const qint64 OpenUs=Timer.nsecsElapsed()/1000;
            View=Window->GetActiveView();
            QVERIFY(View);
            Widget=View->GetWidget();
            QVERIFY(Widget);
            const auto& Models=lcGetActiveProject()->GetModels();
            QCOMPARE(Models.size(),size_t(5));
            QCOMPARE(Models[0]->GetPieces().size(),size_t(4));
            for(size_t Index=1;Index<Models.size();Index++)
                QCOMPARE(Models[Index]->GetPieces().size(),size_t(Count/4));
            QSignalSpy Swapped(Widget,&QOpenGLWidget::frameSwapped);
            View->SetViewpoint(lcViewpoint::Home);
            View->ZoomExtents();
            Widget->update();
            QVERIFY(Swapped.count() || Swapped.wait(5000));
            const qint64 FrameUs=Timer.nsecsElapsed()/1000;
            QImage First=Widget->grabFramebuffer();
            QVERIFY(!First.isNull());
            qInfo()<<"D10_RENDER_FRAME"<<Count<<Stage<<"open_us"<<OpenUs<<"first_swap_us"<<FrameUs
                   <<"size"<<First.size()<<"dpr"<<Widget->GetDeviceScale();
            if(qstrcmp(Stage,"reopen")!=0)
                continue;

            QList<qint64> Samples;
            for(int Index=0;Index<12;Index++)
            {
                View->SetViewpoint(Index%2?lcViewpoint::Home:lcViewpoint::Front);
                View->ZoomExtents();
                Timer.restart();
                const QImage Frame=Widget->grabFramebuffer();
                Samples.push_back(Timer.nsecsElapsed()/1000);
                QVERIFY(!Frame.isNull());
                if(Index==0)
                    QVERIFY(Frame!=First);
            }
            std::sort(Samples.begin(),Samples.end());
            qInfo()<<"D10_RENDER_REDRAW"<<Count<<"min_us"<<Samples.front()
                   <<"median_us"<<(Samples[5]+Samples[6])/2<<"max_us"<<Samples.back()<<"samples"<<Samples.size();

            // Exercise the conservative dirty flag with all current meshes
            // still resident, then confirm the real rendered pixels are stable.
            View->SetViewpoint(lcViewpoint::Home);
            View->ZoomExtents();
            const QImage Resident=Widget->grabFramebuffer();
            QVERIFY(!Resident.isNull());
            auto* Library=lcGetPiecesLibrary();
            qInfo()<<"D10_RENDER_STAGE"<<Count<<"resident-invalidations";
            Timer.restart();
            for(int Index=0;Index<8;Index++)
            {
                Library->mBuffersDirty=true;
                QCOMPARE(Widget->grabFramebuffer(),Resident);
            }
            qInfo()<<"D10_RENDER_RESIDENT"<<Count<<"elapsed_us"<<Timer.nsecsElapsed()/1000<<"frames"<<8;
        }
    }
    void windowReentryPreservesScene_data()
    {
        QTest::addColumn<bool>("Minimize");
        QTest::newRow("hidden") << false;
        QTest::newRow("minimized") << true;
    }
    void windowReentryPreservesScene()
    {
        QFETCH(bool, Minimize);
        lcPiece* Piece = Model->GetPieces()[0].get();
        const lcVector3 Position = Piece->mModelWorld.GetTranslation();
        for (QSize Size : {QSize(850, 620), QSize(1300, 850)})
        {
            Window->resize(Size);
            QCoreApplication::processEvents();
            View->SetViewpoint(lcViewpoint::Home);
            View->ZoomExtents();
            Window->SetTool(lcTool::Select);
            const QPointF P = center();
            mouse(QEvent::MouseButtonPress, P, Qt::RightButton, Qt::RightButton);
            QVERIFY(View->IsTracking());
            if (Minimize)
            {
                Window->showMinimized();
                QTRY_VERIFY(Window->isMinimized());
            }
            else
                Window->hide();
            QTRY_VERIFY(!View->IsTracking());

            Window->showNormal();
            Window->raise();
            Window->activateWindow();
            QVERIFY(QTest::qWaitForWindowExposed(Window.get()));
            QApplication::setActiveWindow(Window.get());
            Widget->setFocus();
            QTRY_VERIFY(Widget->hasFocus());
            const lcVector3 Camera = View->GetCamera()->mPosition;
            mouse(QEvent::MouseMove, P + QPointF(30, 20), Qt::NoButton, Qt::NoButton);
            mouse(QEvent::MouseButtonRelease, P, Qt::RightButton, Qt::NoButton);
            QCOMPARE(View->GetCamera()->mPosition, Camera);
            QCOMPARE(View->GetWidth(), int(Widget->width() * Widget->GetDeviceScale()));
            QCOMPARE(View->GetHeight(), int(Widget->height() * Widget->GetDeviceScale()));
            QVERIFY(!Widget->grabFramebuffer().isNull());

            const auto& Bounds = Piece->mPieceInfo->GetBoundingBox();
            const QPointF Pick = fromView(View->ProjectPoint(
                lcMul31((Bounds.Min + Bounds.Max) / 2.0f, Piece->mModelWorld)));
            mouse(QEvent::MouseButtonPress, Pick, Qt::LeftButton, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, Pick, Qt::LeftButton, Qt::NoButton);
            QCOMPARE(Model->GetFocusObject(), static_cast<lcObject*>(Piece));
            QTest::keyClick(Widget, Qt::Key_Right);
            QVERIFY(Piece->mModelWorld.GetTranslation() != Position);
            QTest::keySequence(Widget, QKeySequence("Ctrl+Z"));
            QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(), Position);
            Model->SetSaved();
            Piece = Model->GetPieces()[0].get();
        }
    }
    void projectSwitchRebuildsPartsAfterCleanup()
    {
        QTemporaryDir Files;
        QVERIFY(Files.isValid());
        const QString OldPath = Files.filePath("old.mpd");
        const QString NewPath = Files.filePath("medium.mpd");
        QVERIFY(QFile::copy(QFINDTESTDATA("../fixtures/input-switch.mpd"), OldPath));
        QVERIFY(QFile::copy(QFINDTESTDATA("../../docs/evidence/member-a/2026-11-05/d10-2000.mpd"), NewPath));

        auto* Library = lcGetPiecesLibrary();
        auto* Panel = Window->GetPartSelectionWidget();
        auto* Parts = Panel->findChild<lcPartSelectionListView*>();
        QVERIFY(Parts);
        auto* List = Parts->GetListModel();
        Panel->SetCategory(lcPartCategoryType::AllParts, 0);

        for (int Round = 0; Round < 3; Round++)
        {
            QVERIFY(Window->OpenProject(OldPath));
            lcModel* OldChild = lcGetActiveProject()->GetModel(QStringLiteral("child.ldr"));
            QVERIFY(OldChild);
            PieceInfo* OldInfo = OldChild->GetPieceInfo();
            QVERIFY(List->GetPieceInfoIndex(OldInfo).isValid());

            // Inspect the real list as soon as OpenProject returns, before an
            // invalid row can be dereferenced by a paint or a tooltip.
            QVERIFY(Window->OpenProject(NewPath));
            QCOMPARE(lcGetActiveProject()->GetModels().size(), size_t(5));
            QVERIFY(!Library->FindPiece("child.ldr", lcGetActiveProject(), false, false));
            QVERIFY2(!List->GetPieceInfoIndex(OldInfo).isValid(),
                     "All Parts retained a submodel descriptor removed by project cleanup");

            std::vector<PieceInfo*> PartsInLibrary;
            Library->GetParts(PartsInLibrary);
            const QSet<PieceInfo*> Live(PartsInLibrary.begin(), PartsInLibrary.end());
            for (int Row = 0; Row < List->rowCount(); Row++)
                QVERIFY(Live.contains(List->GetPieceInfo(Row)));

            PieceInfo* NewChild = lcGetActiveProject()->GetModels()[1]->GetPieceInfo();
            QVERIFY(List->GetPieceInfoIndex(NewChild).isValid());
        }

        // Ask the real list delegate to render the new project's thumbnail.
        auto* Dock = Window->findChild<QDockWidget*>(QStringLiteral("PartsToolbar"));
        QVERIFY(Dock);
        Dock->show();
        Panel->SetCategory(lcPartCategoryType::Submodels, 0);
        Parts->SetMediumIcons();
        QTRY_VERIFY(List->rowCount() == 4);
        QTRY_VERIFY(List->data(List->index(0, 0), Qt::DecorationRole).canConvert<QPixmap>());
        QVERIFY(!List->data(List->index(0, 0), Qt::DecorationRole).value<QPixmap>().isNull());
    }
    void alphaSwitchSequence_data()
    {
        QTest::addColumn<bool>("PartsVisible");
        QTest::newRow("canvas-only")<<false;
        QTest::newRow("parts-visible")<<true;
    }
    void alphaSwitchSequence()
    {
        QFETCH(bool,PartsVisible);
        if(PartsVisible)
        {
            auto* Parts=Window->findChild<QDockWidget*>(QStringLiteral("PartsToolbar"));
            QVERIFY(Parts); Parts->show();
        }
        QTemporaryDir Files; QVERIFY(Files.isValid());
        const QString Ldr=Files.filePath("baseline.ldr"), Mpd=Files.filePath("assembly.mpd");
        QVERIFY(QFile::copy(QFINDTESTDATA("../fixtures/render-baseline.ldr"),Ldr));
        QVERIFY(QFile::copy(QFINDTESTDATA("../fixtures/input-switch.mpd"),Mpd));
        int Heartbeats=0; QTimer Heartbeat;
        connect(&Heartbeat,&QTimer::timeout,this,[&]() {Heartbeats++;}); Heartbeat.start(1);
        for(int Iteration=0;Iteration<10;Iteration++)
        {
            for(const QString& Input : {Ldr,Mpd})
            {
                QVERIFY(openAndRefresh(Input)); QTRY_VERIFY(Widget->isValid());
                const bool IsMpd=Input==Mpd;
                QCOMPARE(lcGetActiveProject()->GetModels().size(),size_t(IsMpd?2:1));
                QCOMPARE(Model->GetPieces().size(),size_t(2));
                View->SetViewpoint(lcViewpoint::Home); View->ZoomExtents(); Window->SetTool(lcTool::Select);
                lcPiece* Piece=Model->GetPieces()[0].get();
                const auto& Bounds=Piece->mPieceInfo->GetBoundingBox();
                const QPointF P=fromView(View->ProjectPoint(lcMul31((Bounds.Min+Bounds.Max)/2.0f,Piece->mModelWorld)));
                mouse(QEvent::MouseButtonPress,P,Qt::LeftButton,Qt::LeftButton);
                mouse(QEvent::MouseButtonRelease,P,Qt::LeftButton,Qt::NoButton);
                QCOMPARE(Model->GetFocusObject(),static_cast<lcObject*>(Piece));
                const lcVector3 Before=Piece->mModelWorld.GetTranslation();
                const quint32 Colour=Piece->GetColorCode();
                // Picking can process native activation events. Establish the
                // Qt shortcut context immediately before injected key input.
                QApplication::setActiveWindow(Window.get()); Widget->setFocus();
                QVERIFY(Widget->hasFocus());
                QTest::keyClick(Widget,Qt::Key_Right);
                const lcVector3 Moved=Model->GetPieces()[0]->mModelWorld.GetTranslation();
                QVERIFY(Moved!=Before); QVERIFY(lcGetActiveProject()->IsModified());
                QTest::keySequence(Widget,QKeySequence("Ctrl+Z"));
                QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Before);
                QTest::keySequence(Widget,QKeySequence("Ctrl+Y"));
                QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Moved);
                const QString Output=Files.filePath(IsMpd?"edited.mpd":"edited.ldr");
                QVERIFY(Window->SaveProject(Output)); QVERIFY(!lcGetActiveProject()->IsModified());
                QVERIFY(openAndRefresh(Output));
                QCOMPARE(lcGetActiveProject()->GetModels().size(),size_t(IsMpd?2:1));
                QCOMPARE(Model->GetPieces()[0]->mModelWorld.GetTranslation(),Moved);
                QCOMPARE(Model->GetPieces()[0]->GetColorCode(),Colour);
                if(IsMpd)
                {
                    lcGetActiveProject()->SetActiveModel(1,true);
                    View=Window->GetActiveView(); Widget=View->GetWidget(); Model=View->GetActiveModel();
                    QCOMPARE(Model->GetPieces().size(),size_t(1));
                    lcGetActiveProject()->SetActiveModel(0,true);
                    View=Window->GetActiveView(); Widget=View->GetWidget(); Model=View->GetActiveModel();
                    QCOMPARE(Model->GetPieces().size(),size_t(2));
                }
                QMenu* Menu=Window->menuBar()->actions().front()->menu(); QVERIFY(Menu);
                Menu->popup(Widget->mapToGlobal(QPoint(15,15))); QTRY_VERIFY(Menu->isVisible());
                QTest::keyClick(Menu,Qt::Key_Escape); QTRY_VERIFY(!Menu->isVisible());
                QApplication::setActiveWindow(Window.get()); Widget->setFocus();
                const int Previous=Heartbeats; QTest::qWait(5); QVERIFY(Heartbeats>Previous);
                QVERIFY(!View->IsTracking()); QVERIFY(!lcGetActiveProject()->IsModified());
                QVERIFY(!Widget->grabFramebuffer().isNull());
                qInfo()<<"D8_SWITCH"<<Iteration<<(IsMpd?"MPD":"LDR")<<"parts"<<PartsVisible
                       <<"heartbeat"<<Heartbeats<<"DPR"<<Widget->GetDeviceScale();
            }
        }
        capture(PartsVisible?"d8-switch-parts":"d8-switch-canvas");
    }
    void staleRightReleaseDoesNotOpenMenu()
    {
        const QPointF P=center(); Window->SetTool(lcTool::Select);
        mouse(QEvent::MouseButtonPress,P,Qt::RightButton,Qt::RightButton);
        QFocusEvent Lost(QEvent::FocusOut); QApplication::sendEvent(Widget,&Lost);
        bool PopupOpened=false; QTimer ClosePopup;
        connect(&ClosePopup,&QTimer::timeout,this,[&]() { if(QWidget* Popup=QApplication::activePopupWidget()) {PopupOpened=true;Popup->close();} });
        ClosePopup.start(1);
        mouse(QEvent::MouseButtonRelease,P,Qt::RightButton,Qt::NoButton);
        QVERIFY(!PopupOpened); QVERIFY(!View->IsTracking());
    }
};
int main(int argc,char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("LeoCAD-MemberA-Tests");
    QCoreApplication::setApplicationName("ViewInput");
    QTemporaryDir Settings; if(!Settings.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,Settings.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,Settings.path());
    lcApplication App(argc,argv);
    lcLoadDefaultKeyboardShortcuts(); lcLoadDefaultMouseShortcuts();
    if(!lcContext::InitializeRenderer()) return 2;
    int Result;
    {
        lcPiecesLibrary Library; App.mLibrary=&Library;
        if(!Library.Load(QStringLiteral(":/assets/ldraw-lite/library-lite.zip"),false)) return 2;
        ViewInputTests Tests; Result=QTest::qExec(&Tests,argc,argv);
    }
    App.mLibrary=nullptr;
    lcContext::ShutdownRenderer(); return Result;
}
#include "tst_view_input.moc"
