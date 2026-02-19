#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include <QVariantList>
#include <git2.h>
#include <iostream>
#include <vector>
#include <QDebug>

class DiffBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList diffData READ diffData NOTIFY diffDataChanged)
public:
    explicit DiffBridge(QObject *parent = nullptr) : QObject(parent) {}
    QVariantList diffData() const { return m_diffData; }

    void loadDiff(git_repository *repo, const QString &filePath) {
        if (!repo) return;
        m_diffData.clear();

        git_diff *diff = nullptr;
        git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
        // استفاده از قوی‌ترین الگوریتم‌های Git
        opts.flags |= GIT_DIFF_PATIENCE | GIT_DIFF_INDENT_HEURISTIC | GIT_DIFF_MINIMAL;

        QByteArray pathBytes = filePath.toUtf8();
        const char* path = pathBytes.constData();
        opts.pathspec.strings = const_cast<char**>(&path);
        opts.pathspec.count = 1;

        if (git_diff_index_to_workdir(&diff, repo, nullptr, &opts) != 0) return;

        struct RawLine { char origin; int old_no; int new_no; QString content; };
        std::vector<RawLine> rawLines;

        git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, [](
                                                        const git_diff_delta*, const git_diff_hunk*, const git_diff_line *line, void *payload) -> int {
            auto *vec = static_cast<std::vector<RawLine>*>(payload);
            if (line->origin == ' ' || line->origin == '+' || line->origin == '-') {
                QString content = QString::fromUtf8(line->content, line->content_len);
                content.remove('\n').remove('\r');
                vec->push_back({line->origin, line->old_lineno, line->new_lineno, content});
            }
            return 0;
        }, &rawLines);

        // --- الگوریتم هوشمند Pairing برای تراز کردن دقیق ---
        for (size_t i = 0; i < rawLines.size(); ++i) {
            QVariantMap map;

            // حالت اول: تغییر در یک خط (حذف و اضافه پشت سر هم)
            if (rawLines[i].origin == '-' && (i + 1) < rawLines.size() && rawLines[i+1].origin == '+') {
                map["type"] = 3; // Modified
                map["content"] = rawLines[i].content;    // محتوای قدیمی (چپ)
                map["contentNew"] = rawLines[i+1].content; // محتوای جدید (راست)
                map["oldLine"] = rawLines[i].old_no;
                map["newLine"] = rawLines[i+1].new_no;
                i++; // رد کردن خط بعدی چون جفت شد
            }
            // حالت دوم: فقط حذف شده
            else if (rawLines[i].origin == '-') {
                map["type"] = 2;
                map["content"] = rawLines[i].content;
                map["oldLine"] = rawLines[i].old_no;
                map["newLine"] = -1;
            }
            // حالت سوم: فقط اضافه شده
            else if (rawLines[i].origin == '+') {
                map["type"] = 1;
                map["content"] = rawLines[i].content;
                map["oldLine"] = -1;
                map["newLine"] = rawLines[i].new_no;
            }
            // حالت چهارم: بدون تغییر (Context)
            else {
                map["type"] = 0;
                map["content"] = rawLines[i].content;
                map["oldLine"] = rawLines[i].old_no;
                map["newLine"] = rawLines[i].new_no;
            }
            m_diffData.append(map);
        }

        git_diff_free(diff);
        emit diffDataChanged();
    }

signals:
    void diffDataChanged();
private:
    QVariantList m_diffData;
};

QVariantList getCommitFileChanges(git_repository* repo, const QString &commitHash)
{
    QVariantList fileList;

    qDebug() << "------------------------------------------";
    qDebug() << "[GitTask] Start processing commit:" << commitHash;

    git_object *commitObj = nullptr;
    git_commit *commit = nullptr;
    git_commit *parent = nullptr;
    git_tree *commitTree = nullptr;
    git_tree *parentTree = nullptr;
    git_diff *diff = nullptr;

    // ۱. پیدا کردن آبجکت کامیت
    if (git_revparse_single(&commitObj, repo, commitHash.toUtf8().constData()) != 0) {
        qWarning() << "[GitError] Could not find commit for hash:" << commitHash;
        return fileList;
    }

    commit = reinterpret_cast<git_commit*>(commitObj);

    // ۲. گرفتن درخت کامیت فعلی
    if (git_commit_tree(&commitTree, commit) != 0) {
        qWarning() << "[GitError] Failed to retrieve tree for commit.";
        git_object_free(commitObj);
        return fileList;
    }

    // ۳. پیدا کردن والد (اگر وجود داشته باشد)
    if (git_commit_parentcount(commit) > 0) {
        if (git_commit_parent(&parent, commit, 0) == 0) {
            git_commit_tree(&parentTree, parent);
            qDebug() << "[GitInfo] Parent commit found and tree extracted.";
        }
    } else {
        qDebug() << "[GitInfo] This is an initial commit (no parent).";
    }

    // ۴. مقایسه دو درخت (Diff)
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
    // libgit2 به صورت پیش‌فرض برای درخت‌ها Recursive عمل می‌کند

    if (git_diff_tree_to_tree(&diff, repo, parentTree, commitTree, &opts) == 0) {
        size_t num_deltas = git_diff_num_deltas(diff);
        qDebug() << "[GitTask] Files changed in this commit:" << num_deltas;

        for (size_t i = 0; i < num_deltas; ++i) {
            const git_diff_delta *delta = git_diff_get_delta(diff, i);

            // استخراج آمار تغییرات خطوط (Patch)
            git_patch *patch = nullptr;
            size_t add = 0, del = 0;
            if (git_patch_from_diff(&patch, diff, i) == 0) {
                git_patch_line_stats(nullptr, &add, &del, patch);
                git_patch_free(patch);
            }

            QVariantMap fileMap;
            QString path = QString::fromUtf8(delta->new_file.path);
            fileMap["filePath"] = path;
            fileMap["additions"] = static_cast<int>(add);
            fileMap["deletions"] = static_cast<int>(del);

            QString statusChar;
            switch (delta->status) {
            case GIT_DELTA_ADDED:     statusChar = "A"; break;
            case GIT_DELTA_DELETED:   statusChar = "D"; break;
            case GIT_DELTA_MODIFIED:  statusChar = "M"; break;
            case GIT_DELTA_RENAMED:   statusChar = "R"; break;
            default:                  statusChar = "U"; break;
            }
            fileMap["status"] = statusChar;

            // لاگ برای هر فایل
            qDebug() << "   File:" << path << "[" << statusChar << "]" << " +:" << add << " -:" << del;

            fileList.append(fileMap);
        }
    } else {
        qWarning() << "[GitError] Failed to generate diff between trees.";
    }

    qDebug() << "[GitTask] Finished. Total files sent to UI:" << fileList.count();
    qDebug() << "------------------------------------------";

    // آزادسازی منابع
    if (diff) git_diff_free(diff);
    if (parentTree) git_tree_free(parentTree);
    if (commitTree) git_tree_free(commitTree);
    if (parent) git_commit_free(parent);
    if (commitObj) git_object_free(commitObj);

    return fileList;
}

#include "main.moc"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    git_libgit2_init();

    std::string repo_path = "/home/yasin/test-projects/TestRepo";
    git_repository* repo = nullptr;
    git_repository_open(&repo, repo_path.c_str());

    DiffBridge bridge;
    if (repo) bridge.loadDiff(repo, "Test/main.cpp");

    qmlRegisterSingletonInstance("Test.Bridge", 1, 0, "DiffBridge", &bridge);

    QQmlApplicationEngine engine;
    engine.loadFromModule("Test", "Main");

    std::cout << "#######\n\n";
    getCommitFileChanges(repo, "004194508acece9bb3eff98a9d18b97a9e426a40");

    int res = app.exec();
    if (repo) git_repository_free(repo);
    git_libgit2_shutdown();
    return res;
}
