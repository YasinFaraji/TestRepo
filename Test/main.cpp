#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <git2.h>
#include <stdio.h>
#include <string.h>

struct GitDeleter {
    void operator()(git_repository* r) { git_repository_free(r); }
    void operator()(git_branch_iterator* i) { git_branch_iterator_free(i); }
    void operator()(git_reference* r) { git_reference_free(r); }
};

struct DiffLine {
    char type;
    int oldLineNumber;
    int newLineNumber;
    std::string text;
};

int diff_line_callback(
    const git_diff_delta *delta,
    const git_diff_hunk *hunk,
    const git_diff_line *line,
    void *payload)
{
    auto *diffList = static_cast<std::vector<DiffLine>*>(payload);

    std::string lineContent(line->content, line->content_len);

    diffList->push_back({
        line->origin,
        line->old_lineno,
        line->new_lineno,
        lineContent
    });

    return 0;
}


void runDiffTest(git_repository *repo, const std::string &filePath) {
    git_diff *diff = nullptr;
    git_diff_options opts = GIT_DIFF_OPTIONS_INIT;

    const char* path = filePath.c_str();
    opts.pathspec.strings = const_cast<char**>(&path);
    opts.pathspec.count = 1;

    if (git_diff_index_to_workdir(&diff, repo, nullptr, &opts) != 0) {
        std::cerr << "Failed to create diff." << std::endl;
        return;
    }

    std::vector<DiffLine> allLines;

    git_diff_print(
        diff,
        GIT_DIFF_FORMAT_PATCH,
        diff_line_callback,
        &allLines
    );

    std::cout << "--- Diff Report for: " << filePath << " ---" << std::endl;
    for (const auto& line : allLines) {
        std::cout << "Old: " << line.oldLineNumber
                  << " | New: " << line.newLineNumber
                  << " | " << line.type << " " << line.text;
    }

    git_diff_free(diff);
}


bool renameBranch(git_repository* repo, const QString &oldName, const QString &newName)
{
    git_reference* branchRef = nullptr;
    git_reference* newRef = nullptr;
    bool success = false;

    int error = git_branch_lookup(&branchRef, repo, oldName.toUtf8().constData(), GIT_BRANCH_LOCAL);

    if (error == 0) {
        error = git_branch_move(&newRef, branchRef, newName.toUtf8().constData(), 0);

        if (error == 0) {
            qDebug() << "GitWrapperCPP: Renamed" << oldName << "to" << newName;
            success = true;
        } else {
            const git_error* e = git_error_last();
            qWarning() << "GitWrapperCPP: Rename failed:" << (e ? e->message : "Unknown");
        }
    }

    if (newRef) git_reference_free(newRef);
    if (branchRef) git_reference_free(branchRef);

    return success;
}


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Test", "Main");

    git_libgit2_init();

    // std::string repo_path = "/home/sadadpsp/workspace/NodeLink";
    std::string repo_path = "/home/sadadpsp/test-project/TestRepo";
    git_repository* repo_ptr = nullptr;

    if (git_repository_open(&repo_ptr, repo_path.c_str()) != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error opening repository: " << (e ? e->message : "Unknown error") << std::endl;
        git_libgit2_shutdown();
        return 1;
    }

    std::unique_ptr<git_repository, GitDeleter> repo(repo_ptr);

    std::cout << "<####" << std::endl << std::endl;

    runDiffTest(repo_ptr, "README.md");

    std::cout << "\n\n####>" << std::endl << std::endl;

    git_libgit2_shutdown();

    return app.exec();
}
