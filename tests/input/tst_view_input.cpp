#include "lc_global.h"
#include <QtTest>
#include "lc_application.h"
#include "lc_context.h"
#include "lc_library.h"
#include "lc_mainwindow.h"
#include "lc_model.h"
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
