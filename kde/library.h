#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>

/* One Workshop item the picker can show. `note` is the honest reason a live
   preview is not started. An empty note means the files needed for that type
   are present. */
struct Paper {
    QString id;
    QString title;
    QString type;
    QString dir;
    QString preview;
    QString thumb;
    QString page;
    QString note;
    QStringList tags;
    bool gif = false;
    bool liveOk = false;
};

class Library : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int shown READ shownCount NOTIFY shownChanged)
    Q_PROPERTY(int total READ totalCount NOTIFY totalChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)
    Q_PROPERTY(QString tagFilter READ tagFilter WRITE setTagFilter NOTIFY tagFilterChanged)
    Q_PROPERTY(QStringList tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY statusLineChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        TypeRole,
        DirRole,
        PreviewRole,
        ThumbRole,
        NoteRole,
        TagsRole,
        GifRole,
        LiveRole,
        PageRole,
        ActiveRole
    };

    explicit Library(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool scanning() const { return m_scanning; }
    int shownCount() const { return m_shown.size(); }
    int totalCount() const { return m_all.size(); }
    QString query() const { return m_query; }
    QString typeFilter() const { return m_type; }
    QString tagFilter() const { return m_tag; }
    QStringList tags() const { return m_tags; }
    QString statusLine() const { return m_status; }

    void setQuery(const QString& query);
    void setTypeFilter(const QString& type);
    void setTagFilter(const QString& tag);
    void setActiveText(const QHash<QString, QString>& byId);

    /* `onlyWorkshop` and `vdfPath` replace the Steam search. Empty means
       the default libraryfolders.vdf paths plus BENDWALL_WORKSHOP. */
    void setRoots(const QStringList& onlyWorkshop, const QString& vdfPath);
    Q_INVOKABLE void start();
    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE QVariantMap paper(const QString& id) const;
    bool liveOkAt(int row) const;
    QString noteAt(int row) const;

signals:
    void scanningChanged();
    void shownChanged();
    void totalChanged();
    void queryChanged();
    void typeFilterChanged();
    void tagFilterChanged();
    void tagsChanged();
    void statusLineChanged();
    void scanFinished();

private:
    void addItem(const QVariantMap& item);
    void finishScan(const QString& status);
    void rebuildShown();
    bool passes(const Paper& paper) const;
    void recomputeTags();

    QList<Paper> m_all;
    QList<int> m_shown;
    QHash<QString, QString> m_active;
    QStringList m_only;
    QString m_vdf;
    QString m_query;
    QString m_type;
    QString m_tag;
    QStringList m_tags;
    QString m_status;
    bool m_scanning = false;
    bool m_started = false;
};
