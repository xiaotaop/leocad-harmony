#include "lc_renderimage.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QTest>

class ReadbackTests : public QObject
{
    Q_OBJECT
private slots:
    void copyFormats_data()
    {
        QTest::addColumn<int>("Format");
        for (QImage::Format Format : {QImage::Format_ARGB32, QImage::Format_RGBA8888,
            QImage::Format_ARGB32_Premultiplied, QImage::Format_RGBA8888_Premultiplied, QImage::Format_RGB888})
            QTest::newRow(qPrintable(QString::number(int(Format)))) << int(Format);
    }
    void copyFormats()
    {
        QFETCH(int, Format);
        QImage Source(7, 5, QImage::Format_ARGB32);
        const QColor Colors[] = {QColor(201,26,9), QColor(0,85,191), QColor(180,210,227), QColor(255,207,0)};
        for (int y=0; y<Source.height(); y++)
            for (int x=0; x<Source.width(); x++)
                Source.setPixelColor(x,y, Colors[(x+2*y)%4]);
        Source = Source.convertToFormat(QImage::Format(Format));
        // Extra stride padding is deliberate. Copy a non-origin rectangle into
        // an odd-width target, with a sentinel border that must remain intact.
        const qsizetype Stride = Source.bytesPerLine()+16;
        QByteArray Bytes(Stride*Source.height(), char(0x5a));
        QImage Padded(reinterpret_cast<uchar*>(Bytes.data()), Source.width(), Source.height(), Stride, Source.format());
        for (int y=0;y<Source.height();y++) std::memcpy(Padded.scanLine(y),Source.constScanLine(y),Source.bytesPerLine());
        QImage Destination(9,7,QImage::Format_ARGB32); Destination.fill(Qt::magenta);
        lcCopyRenderImageTile(Padded,Destination,QRect(1,1,5,3),QPoint(2,2));
        for (int y=0;y<Destination.height();y++)
            for (int x=0;x<Destination.width();x++)
                QCOMPARE(Destination.pixelColor(x,y), QRect(2,2,5,3).contains(x,y) ? Source.pixelColor(x-1,y-1) : QColor(Qt::magenta));
    }
    void oldRawCopySwapsRedAndBlue()
    {
        QImage Source(3,1,QImage::Format_RGBA8888);
        Source.setPixelColor(0,0,QColor(201,26,9));
        Source.setPixelColor(1,0,QColor(0,85,191));
        Source.setPixelColor(2,0,QColor(180,210,227));
        QImage Old(3,1,QImage::Format_ARGB32);
        std::memcpy(Old.bits(),Source.constBits(),12);
        QVERIFY(Old.pixelColor(0,0)!=Source.pixelColor(0,0));
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        QCOMPARE(Old.pixelColor(0,0),QColor(9,26,201));
        QCOMPARE(Old.pixelColor(1,0),QColor(191,85,0));
        QCOMPARE(Old.pixelColor(2,0),QColor(227,210,180));
#endif
        QImage Fixed(3,1,QImage::Format_ARGB32);
        lcCopyRenderImageTile(Source,Fixed,Source.rect(),QPoint());
        for(int x=0;x<3;x++) QCOMPARE(Fixed.pixelColor(x,0).rgba(),Source.pixelColor(x,0).rgba());
    }
    void alphaIsPreserved_data()
    {
        QTest::addColumn<int>("Format");
        QTest::newRow("rgba") << int(QImage::Format_RGBA8888);
        QTest::newRow("premultiplied") << int(QImage::Format_RGBA8888_Premultiplied);
    }
    void alphaIsPreserved()
    {
        QFETCH(int, Format);
        QImage Source(3,1,QImage::Format_ARGB32);
        for(int x=0;x<3;x++) Source.setPixelColor(x,0,QColor(200,50,20,x*127));
        Source=Source.convertToFormat(QImage::Format(Format));
        QImage Fixed(3,1,QImage::Format_ARGB32);
        lcCopyRenderImageTile(Source,Fixed,Source.rect(),QPoint());
        for(int x=0;x<3;x++) QCOMPARE(Fixed.pixelColor(x,0).rgba(),Source.pixelColor(x,0).rgba());
    }
    void actualFramebufferReadback_data()
    {
        QTest::addColumn<int>("Samples");
        QTest::newRow("single-sample") << 0;
        QTest::newRow("multisample") << 4;
    }
    void actualFramebufferReadback()
    {
        QFETCH(int, Samples);
        QOpenGLContext Context; Context.setFormat(QSurfaceFormat::defaultFormat());
        QVERIFY(Context.create());
        QCOMPARE(Context.isOpenGLES(),qgetenv("LEOCAD_TEST_GLES")=="1");
        QOffscreenSurface Surface; Surface.setFormat(Context.format()); Surface.create();
        QVERIFY(Surface.isValid() && Context.makeCurrent(&Surface));
        QOpenGLFunctions* GL=Context.functions(); GL->initializeOpenGLFunctions();
        QOpenGLFramebufferObjectFormat Format; Format.setAttachment(QOpenGLFramebufferObject::Depth); Format.setSamples(Samples);
        QOpenGLFramebufferObject FBO(7,5,Format); QVERIFY(FBO.isValid() && FBO.bind());
        if(Samples) QVERIFY(FBO.format().samples()>1);
        GL->glEnable(GL_SCISSOR_TEST);
        const QColor Colors[] = {QColor(201,26,9,255),QColor(0,85,191,128),QColor(180,210,227,64),QColor(255,207,0,255)};
        for(int y=0;y<5;y++) for(int x=0;x<7;x++)
        {
            const QColor C=Colors[(x+2*y)%4]; GL->glScissor(x,y,1,1);
            GL->glClearColor(C.redF()*C.alphaF(),C.greenF()*C.alphaF(),C.blueF()*C.alphaF(),C.alphaF()); GL->glClear(GL_COLOR_BUFFER_BIT);
        }
        GL->glDisable(GL_SCISSOR_TEST); FBO.release();
        const QImage Raw=FBO.toImage(); QVERIFY(!Raw.isNull());
        qInfo()<<"actual readback"<<Context.format()<<Raw.format()<<Raw.bytesPerLine()<<"samples"<<FBO.format().samples();
        QImage Fixed(7,5,QImage::Format_ARGB32);
        lcCopyRenderImageTile(Raw,Fixed,Raw.rect(),QPoint());
        for(int y=0;y<5;y++) for(int x=0;x<7;x++)
        {
            const QColor Expected=QColor::fromRgba(qUnpremultiply(qPremultiply(Colors[(x+2*(4-y))%4].rgba()))), Actual=Fixed.pixelColor(x,y);
            QVERIFY(qAbs(Actual.red()-Expected.red())<=1 && qAbs(Actual.green()-Expected.green())<=1
                && qAbs(Actual.blue()-Expected.blue())<=1 && qAbs(Actual.alpha()-Expected.alpha())<=1);
        }
        QCOMPARE(GL->glGetError(),GLenum(GL_NO_ERROR));
    }
};
int main(int argc,char** argv)
{
    const bool GLES=qgetenv("LEOCAD_TEST_GLES")=="1";
    QSurfaceFormat Format; Format.setRenderableType(GLES?QSurfaceFormat::OpenGLES:QSurfaceFormat::OpenGL);
    Format.setVersion(GLES?3:2,GLES?0:1); Format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(Format);
    QGuiApplication App(argc,argv); ReadbackTests Tests; return QTest::qExec(&Tests,argc,argv);
}
#include "tst_readback.moc"
