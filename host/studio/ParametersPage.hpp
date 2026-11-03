#pragma once
#include <QWidget>

#include <map>
#include <vector>

class DriveController;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/// All parameters grouped by category. Normal mode: read and export. Expert mode: edit
/// (changes are collected and written with Apply), import, save in drive, factory defaults.
class ParametersPage : public QWidget {
    Q_OBJECT
public:
    explicit ParametersPage(DriveController* ctl, QWidget* parent = nullptr);

private:
    void build();
    void refresh();
    void onExpertMode(bool on);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void applyFilter();
    void apply();
    void discard();
    void exportFile();
    void importFile();
    void saveInDrive();
    void loadDefaults();
    QString formatValue(int idx, float v) const;
    bool editable(int idx) const;

    DriveController* ctl_;
    QTreeWidget* tree_ = nullptr;
    QLineEdit* filter_ = nullptr;
    QPushButton* readAll_ = nullptr;
    QPushButton* apply_ = nullptr;
    QPushButton* discard_ = nullptr;
    QPushButton* export_ = nullptr;
    QPushButton* import_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* defaults_ = nullptr;
    QLabel* info_ = nullptr;

    std::vector<QTreeWidgetItem*> items_;   ///< per focp_param_idx_t
    std::map<QString, QTreeWidgetItem*> groups_;
    std::map<int, float> pending_;          ///< edited, not yet written
    bool updating_ = false;
};
