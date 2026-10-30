#pragma once

#include "lc_thumbnailmanager.h"

class lcPartSelectionListModel;
class lcPartSelectionListView;
class lcPartSelectionWidget;

enum class lcPartCategoryType
{
	AllParts,
	PartsInUse,
	Submodels,
	Palette,
	Category,
	Custom,
	Count
};

enum class lcPartCategoryRole
{
	Type = Qt::UserRole,
	Index
};

enum class lcPartFilterType
{
	Default,
	RegularExpression,
	Count
};

struct lcPartPalette
{
	QString Name;
	std::vector<std::string> Parts;
};

class lcPartSelectionItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	lcPartSelectionItemDelegate(QObject* Parent, lcPartSelectionListModel* ListModel)
		: QStyledItemDelegate(Parent), mListModel(ListModel)
	{
	}

	void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
	QSize sizeHint(const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;

protected:
	lcPartSelectionListModel* mListModel;
};

struct lcPartSelectionListModelEntry
{
	PieceInfo* Info = nullptr;
	std::string Description;
	int ColorIndex = -1;
	QPixmap Pixmap;
	lcPartThumbnailId ThumbnailId = lcPartThumbnailId::Invalid;
};

class lcPartSelectionListModel : public QAbstractListModel
{
	Q_OBJECT

public:
	lcPartSelectionListModel(QObject* Parent);
	~lcPartSelectionListModel();

	int rowCount(const QModelIndex& Parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& Index, int Role = Qt::DisplayRole) const override;
	QVariant headerData(int Section, Qt::Orientation Orientation, int Role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& Index) const override;

	QModelIndex GetPieceInfoIndex(PieceInfo* Info) const;

	PieceInfo* GetPieceInfo(const QModelIndex& Index) const
	{
		return Index.isValid() ? mParts[Index.row()].Info : nullptr;
	}

	PieceInfo* GetPieceInfo(int Row) const
	{
		return mParts[Row].Info;
	}

	bool GetShowDecoratedParts() const
	{
		return mShowDecoratedParts;
	}

	bool GetShowPartAliases() const
	{
		return mShowPartAliases;
	}

	lcPartFilterType GetPartFilterType() const
	{
		return mPartFilterType;
	}

	bool GetCaseSensitiveFilter() const
	{
		return mCaseSensitiveFilter;
	}

	bool GetFileNameFilter() const
	{
		return mFileNameFilter;
	}

	bool GetPartDescriptionFilter() const
	{
		return mPartDescriptionFilter;
	}

	int GetIconSize() const
	{
		return mIconSize;
	}

	bool GetShowPartNames() const
	{
		return mShowPartNames;
	}

	int GetColorIndex() const
	{
		return mColorIndex;
	}

	bool IsColorLocked() const
	{
		return mColorLocked;
	}

	bool IsListMode() const
	{
		return mListMode;
	}

	void UpdateThumbnails();
	void SetColorIndex(int ColorIndex);
	void ToggleColorLocked();
	void ToggleListMode();
	void SetCategory(int CategoryIndex);
	void SetModelsCategory();
	void SetPaletteCategory(int SetIndex);
	void SetCurrentModelCategory();
	void SetCustomParts(const std::vector<std::pair<PieceInfo*, std::string>>& Parts, int ColorIndex);
	void SetFilter(const QString& FilterString);
	void RequestThumbnail(int PartIndex);
	void SetShowDecoratedParts(bool Show);
	void SetShowPartAliases(bool Show);
	void SetPartFilterType(lcPartFilterType Option);
	void SetCaseSensitiveFilter(bool Option);
	void SetFileNameFilter(bool Option);
	void SetPartDescriptionFilter(bool Option);
	void SetIconSize(int Size, float DeviceScale);
	void SetShowPartNames(bool Show);

signals:
	// Emitted once the visible set has been recomputed, i.e. whenever the list
	// the user sees may have changed size. SetFilter() is the single place that
	// decides visibility, so this covers the filter box, the options menu, the
	// category switch and the In Use / submodel / palette paths alike.
	void VisibleRowsChanged();

protected slots:
	void ThumbnailReady(lcPartThumbnailId ThumbnailId, QPixmap Pixmap);

protected:
	void ReleaseThumbnails();

	lcPartSelectionListView* mListView;
	std::vector<lcPartSelectionListModelEntry> mParts;
	lcPartFilterType mPartFilterType;
	int mIconSize;
	float mDeviceScale = 1.0f;
	bool mColorLocked;
	int mColorIndex;
	bool mShowPartNames;
	bool mListMode;
	bool mShowDecoratedParts;
	bool mShowPartAliases;
	bool mCaseSensitiveFilter;
	bool mFileNameFilter;
	bool mPartDescriptionFilter;
	QString mFilterString;
};

class lcPartSelectionListView : public QListView
{
	Q_OBJECT

public:
	static constexpr int NoIconSize = 0;
	static constexpr int SmallIconSize = 32;
	static constexpr int MediumIconSize = 64;
	static constexpr int LargeIconSize = 96;
	static constexpr int ExtraLargeIconSize = 192;

	lcPartSelectionListView(QWidget* Parent, lcPartSelectionWidget* PartSelectionWidget);

	void startDrag(Qt::DropActions SupportedActions) override;
	QSize sizeHint() const override;

	void SetCategory(lcPartCategoryType Type, int Index);
	void SetCustomParts(const std::vector<std::pair<PieceInfo*, std::string>>& Parts, int ColorIndex);
	void SetCurrentPart(PieceInfo* Info);

	PieceInfo* GetCurrentPart() const
	{
		return mListModel->GetPieceInfo(currentIndex());
	}

	lcPartSelectionListModel* GetListModel() const
	{
		return mListModel;
	}

	lcPartSelectionWidget* GetPartSelectionWidget() const
	{
		return mPartSelectionWidget;
	}

	PieceInfo* GetContextInfo() const
	{
		return mContextInfo;
	}

	void UpdateInUseCategory();
	void UpdateViewMode();

signals:
	void PartPicked(PieceInfo* Info);

public slots:
	void CustomContextMenuRequested(QPoint Pos);
	void SetNoIcons();
	void SetSmallIcons();
	void SetMediumIcons();
	void SetLargeIcons();
	void SetExtraLargeIcons();
	void ToggleRegularExpressionFilter();
	void TogglePartNames();
	void ToggleDecoratedParts();
	void TogglePartAliases();
	void ToggleCaseSensitiveFilter();
	void ToggleFileNameFilter();
	void TogglePartDescriptionFilter();
	void ToggleListMode();
	void ToggleFixedColor();

protected slots:
	void DoubleClicked(const QModelIndex& Index);

protected:
	void SetIconSize(int Size);
	void SetPartFilterType(lcPartFilterType Option);
	void keyPressEvent(QKeyEvent* KeyEvent) override;

	lcPartSelectionListModel* mListModel = nullptr;
	lcPartSelectionWidget* mPartSelectionWidget = nullptr;
	PieceInfo* mContextInfo = nullptr;
	lcPartCategoryType mCategoryType = lcPartCategoryType::AllParts;
	int mCategoryIndex = 0;
	std::map<std::pair<lcPartCategoryType, int>, int> mLastCategoryRow;
};

class lcPartSelectionWidget : public QWidget
{
	Q_OBJECT

public:
	lcPartSelectionWidget(QWidget* Parent);

	void UpdateThumbnails();
	void SetDefaultPart();
	void UpdateModels();
	void UpdateInUseCategory();
	void UpdateCategories();
	void LoadState(QSettings& Settings);
	void SaveState(QSettings& Settings);
	void DisableIconMode();
	void SetOrientation(Qt::Orientation Orientation);
	void SetCurrentPart(PieceInfo* Info);
	void SetCategory(lcPartCategoryType Type, int Index);
	void SetCustomParts(const std::vector<std::pair<PieceInfo*, std::string>>& Parts, int ColorIndex);

	bool GetIsPopup() const
	{
		return mIsPopup;
	}

	void SetIsPopup(bool IsPopup)
	{
		mIsPopup = IsPopup;

		if (mIsPopup)
			mPartsWidget->setMinimumSize(0, 0);
	}

	int GetColorIndex() const
	{
		return mPartsWidget->GetListModel()->GetColorIndex();
	}

	void SetColorIndex(int ColorIndex)
	{
		mPartsWidget->GetListModel()->SetColorIndex(ColorIndex);
	}

	const std::vector<lcPartPalette>& GetPartPalettes() const
	{
		return mPartPalettes;
	}

	PieceInfo* GetCurrentPart() const
	{
		return mPartsWidget->GetCurrentPart();
	}

	void SetDragEnabled(bool Enabled)
	{
		mPartsWidget->setDragEnabled(Enabled);
	}

	// The empty-state notice and the count label are driven from the model, not
	// from the category code, because the same "nothing to show" state arrives
	// three ways: an empty category, a filter that matches nothing, and a parts
	// library that failed to load. Updating them in one place is what keeps a
	// filtered-out list from being reported as a broken library.
	void RefreshStatus();

	// Turns the banner on or off and attaches the diagnostic behind it.
	//
	// Degraded must describe the library that actually loaded, not the packaged
	// archive's validation result: a valid LEOCAD_LIB or parts-library preference
	// wins before the bundled archive is even tried, and a banner claiming parts
	// are missing would then be false.
	//
	// Problem is the validator's wording, kept for the tooltip and a bug report.
	// It is shown only when Degraded is true; a healthy run keeps it as the
	// explanation of why the bundled archive was skipped, if it was.
	void SetLibraryProblem(const QString& Problem, bool Degraded);

	const QString& GetLibraryProblem() const
	{
		return mLibraryProblem;
	}

	bool IsLibraryDegraded() const
	{
		return mLibraryDegraded;
	}

	void FocusPartFilterWidget() const
	{
		mFilterWidget->setFocus();
		mFilterWidget->selectAll();
	}

signals:
	void PartPicked(PieceInfo* Info);
	void CurrentPartChanged(PieceInfo* Info);

public slots:
	void AddToPalette();
	void RemoveFromPalette();
	void OpenSubmodel();
	void DockLocationChanged(Qt::DockWidgetArea Area);

protected slots:
	void FilterChanged(const QString& Text);
	void FilterCategoriesChanged(const QString& Text);
	void FilterTriggered();
	void FilterCaseTriggered();
	void FilterCategoriesTriggered();
	void CategoryChanged(QTreeWidgetItem* Current, QTreeWidgetItem* Previous);
	void PartViewSelectionChanged(const QModelIndex& Current, const QModelIndex& Previous);
	void PartViewPartPicked(PieceInfo* Info);
	void OptionsMenuAboutToShow();
	void EditPartPalettes();

protected:
	void LoadPartPalettes();
	void SavePartPalettes();

	// Keeps the empty-state overlay the size of the viewport. Done from an event
	// filter rather than by subclassing the view: the overlay is the panel's
	// concern, and the view is shared with the popup picker.
	void PositionEmptyNotice();

	void resizeEvent(QResizeEvent* Event) override;
	bool event(QEvent* Event) override;
	bool eventFilter(QObject* Watched, QEvent* Event) override;

	QTreeWidget* mCategoriesWidget = nullptr;
	QLineEdit* mFilterCategoriesWidget = nullptr;
	QAction* mFilterCategoriesAction = nullptr;
	QAction* mFilterCaseAction = nullptr;
	QLineEdit* mFilterWidget = nullptr;
	QAction* mFilterAction = nullptr;
	lcPartSelectionListView* mPartsWidget = nullptr;
	QSplitter* mSplitter = nullptr;
	QTreeWidgetItem* mAllPartsCategoryItem = nullptr;
	std::vector<lcPartPalette> mPartPalettes;

	// Shown above the list when the packaged library was refused. Hidden in the
	// normal case so the panel looks unchanged on a healthy install.
	QLabel* mLibraryProblemLabel = nullptr;

	// Overlay on the parts viewport, not a layout item: adding it to the layout
	// would shrink the list every time it appeared (the trap recorded in
	// docs/library/D2_精简零件库与资源部署方案.md section 6.2).
	QLabel* mEmptyNotice = nullptr;

	// "1160 parts" under the list. The count is of what is currently visible,
	// so filtering tells the user how much of the library matched.
	QLabel* mCountLabel = nullptr;

	QString mLibraryProblem;
	bool mLibraryDegraded = false;
	bool mIsPopup = false;
};
