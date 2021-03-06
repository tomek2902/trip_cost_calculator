#include "ActivitiesModel.hpp"

#include <set>

#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>

#include "CurrentStateProvider.hpp"

ActivitiesModel::ActivitiesModel(const CurrentStateProviderPtr& currentStateProvider,
    const QSqlQueryCreatorPtr& queryCreator, QObject* parent)
    : QAbstractTableModel(parent), currentStateProvider_(currentStateProvider), queryCreator_(queryCreator)
{
    CurrentStateProvider* stateProvider = dynamic_cast<CurrentStateProvider*>(currentStateProvider);
    QObject::connect(stateProvider, &CurrentStateProvider::currentLoggedUserChange,
        this, &ActivitiesModel::setQuery);
}

int ActivitiesModel::rowCount(const QModelIndex &) const
{
    return data_.size();
}

int ActivitiesModel::columnCount(const QModelIndex &) const
{
    return headers_.length();
}

QVariant ActivitiesModel::data(const QModelIndex &index, int role) const
{
    if (role < Qt::UserRole) return QVariant();

    int columnIndex{0};
    if (role == Qt::UserRole + 1) columnIndex = 1;
    if (role == Qt::UserRole + 2) columnIndex = 2;

    return QVariant(data_[index.row()][columnIndex]);
}

QVariant ActivitiesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role < Qt::UserRole || orientation != Qt::Horizontal) return QVariant();

    return headers_[section];
}

QHash<int,QByteArray> ActivitiesModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[Qt::UserRole] = "data";
    names[Qt::UserRole + 1] = "added_by";
    names[Qt::UserRole + 2] = "description";
    return names;
}

void ActivitiesModel::setQuery()
{
    const auto query = queryCreator_->create();
    query->prepare("WITH gr AS (SELECT DISTINCT group_name FROM groups_relations WHERE user_1=:user OR user_2=:user), "
        "activities AS (SELECT group_name || '_activities' AS groupActivities_tablename FROM gr) "
        "SELECT groupActivities_tablename FROM activities");
    query->bindValue(":user", currentStateProvider_->getCurrentLoggedUser());
    query->exec();

    std::set<Activity> activities;
    const auto secQuery = queryCreator_->create();
    while(query->next())
    {
        secQuery->clear();
        const auto gr_activities = query->value("groupActivities_tablename").toString();
        secQuery->prepare("SELECT data, added_by, description FROM " + gr_activities);
        secQuery->exec();
        while (secQuery->next())
        {
            activities.insert({secQuery->value("data").toString(),
                secQuery->value("added_by").toString(), secQuery->value("description").toString()});
        }
    }

    for (const auto& act : activities)
    {
        data_.push_back({act.data, act.added_by, act.description});
    }
}

GroupActivitiesModel::GroupActivitiesModel(const CurrentStateProviderPtr& currentStateProvider)
    : currentStateProvider_(currentStateProvider)
{
    CurrentStateProvider* stateProvider = dynamic_cast<CurrentStateProvider*>(currentStateProvider);
    QObject::connect(stateProvider, &CurrentStateProvider::currentLoggedUserChange,
        this, &GroupActivitiesModel::setQuery);
    QObject::connect(stateProvider, &CurrentStateProvider::currentGroupChange,
        this, &GroupActivitiesModel::setQuery);
}

void GroupActivitiesModel::setQuery()
{
    if (currentStateProvider_->getCurrentGroup().isEmpty()
        || currentStateProvider_->getCurrentLoggedUser().isEmpty())
    {
        qDebug() << "GroupName or Username not set, unavailable to set query";
        return;
    }
    const QString query =
        "SELECT data, added_by, value, description FROM " + currentStateProvider_->getCurrentGroup() + "_activities";
    BaseQSqlQueryModel::setQuery(query);
}
