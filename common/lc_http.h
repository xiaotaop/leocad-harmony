#pragma once

#if !defined(LC_DISABLE_NETWORK) && !defined(Q_OS_WIN)
#include <QNetworkRequest>
#include <QNetworkReply>
#endif

#ifdef LC_DISABLE_NETWORK

class lcHttpReply : public QObject
{
	Q_OBJECT

public:
	explicit lcHttpReply(QObject* Parent = nullptr)
		: QObject(Parent)
	{
	}

	bool error() const
	{
		return true;
	}

	void abort()
	{
	}

	QByteArray readAll() const
	{
		return {};
	}
};

class lcHttpManager : public QObject
{
	Q_OBJECT

public:
	explicit lcHttpManager(QObject* Owner = nullptr);

	lcHttpReply* DownloadFile(const QString& Url);

signals:
	void DownloadFinished(lcHttpReply* Reply);
};

#elif defined(Q_OS_WIN)

class lcHttpReply : public QThread
{
	Q_OBJECT

public:
	lcHttpReply(QObject* Parent, const QString& URL);

	void run() override;

	bool error() const
	{
		return mError;
	}

	void abort()
	{
		mAbort = true;
	}

	QByteArray readAll() const
	{
		return mBuffer;
	}

protected:
	bool mError;
	bool mAbort;
	QByteArray mBuffer;
	QString mURL;
};

class lcHttpManager : public QObject
{
	Q_OBJECT

public:
	lcHttpManager(QObject* Owner = nullptr);

	lcHttpReply* DownloadFile(const QString& Url);

signals:
	void DownloadFinished(lcHttpReply* Reply);
};

#else

class lcHttpReply : public QNetworkReply
{
	Q_OBJECT

	lcHttpReply(QObject* Parent)
		: QNetworkReply(Parent)
	{
	}
};

class lcHttpManager : public QNetworkAccessManager
{
	Q_OBJECT

public:
	lcHttpManager(QObject* Owner = nullptr);

	lcHttpReply* DownloadFile(const QString& Url);

signals:
	void DownloadFinished(lcHttpReply* Reply);

protected slots:
	void Finished(QNetworkReply* Reply);
};

#endif
