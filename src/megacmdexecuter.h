/**
 * @file src/megacmdexecuter.h
 * @brief MEGAcmd: Executer of the commands
 *
 * (c) 2013 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGAcmd.
 *
 * MEGAcmd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */

#ifndef MEGACMDEXECUTER_H
#define MEGACMDEXECUTER_H

#include "megacmdlogger.h"
#include "megacmdsandbox.h"
#include "listeners.h"

namespace megacmd {
class MegaCmdSandbox;
class MegaCmdMultiTransferListener;
class MegaCmdGlobalTransferListener;

class MegaCmdExecuter
{
private:
    mega::MegaApi *api;
    mega::handle cwd;
    char *session;
    mega::MegaFileSystemAccess *fsAccessCMD;
    MegaCMDLogger *loggerCMD;
    MegaCmdSandbox *sandboxCMD;
    MegaCmdGlobalTransferListener *globalTransferListener;
    std::mutex mtxSyncMap;
    std::mutex mtxWebDavLocations;
    std::mutex mtxFtpLocations;

#ifdef ENABLE_BACKUPS
    std::recursive_mutex mtxBackupsMap;
#endif

    // login/signup e-mail address
    std::string login;

    // signup name
    std::string name;

    //delete confirmation
    std::vector<mega::MegaNode *> nodesToConfirmDelete;


    std::string getNodePathString(mega::MegaNode *n);

public:
    bool signingup;
    bool confirming;
    bool confirmingcancel;
    // link to confirm
    std::string link;

    MegaCmdExecuter(mega::MegaApi *api, MegaCMDLogger *loggerCMD, MegaCmdSandbox *sandboxCMD);
    ~MegaCmdExecuter();

    void updateprompt(mega::MegaApi *api = nullptr);

    // nodes browsing
    void listtrees();
    static bool includeIfIsExported(mega::MegaApi* api, mega::MegaNode * n, void *arg);
    static bool includeIfIsShared(mega::MegaApi* api, mega::MegaNode * n, void *arg);
    static bool includeIfIsPendingOutShare(mega::MegaApi* api, mega::MegaNode * n, void *arg);
    static bool includeIfIsSharedOrPendingOutShare(mega::MegaApi* api, mega::MegaNode * n, void *arg);
    static bool includeIfMatchesPattern(mega::MegaApi* api, mega::MegaNode * n, void *arg);
    static bool includeIfMatchesCriteria(mega::MegaApi* api, mega::MegaNode * n, void *arg);

    bool processTree(mega::MegaNode * n, bool(mega::MegaApi *, mega::MegaNode *, void *), void *( arg ));

    mega::MegaNode* nodebypath(const char* ptr, std::string* user = NULL, std::string* namepart = NULL);
    std::vector <mega::MegaNode*> * nodesbypath(const char* ptr, bool usepcre, std::string* user = NULL);
    void getNodesMatching(mega::MegaNode *parentNode, std::deque<std::string> pathParts, std::vector<mega::MegaNode *> *nodesMatching, bool usepcre);

    std::vector <std::string> * nodesPathsbypath(const char* ptr, bool usepcre, std::string* user = NULL, std::string* namepart = NULL);
    void getPathsMatching(mega::MegaNode *parentNode, std::deque<std::string> pathParts, std::vector<std::string> *pathsMatching, bool usepcre, std::string pathPrefix = "");

    void printTreeSuffix(int depth, std::vector<bool> &lastleaf);
    void dumpNode(mega::MegaNode* n, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, int extended_info, bool showversions = false, int depth = 0, const char* title = NULL);
    void dumptree(mega::MegaNode* n, bool treelike, std::vector<bool> &lastleaf, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, int recurse, int extended_info, bool showversions = false, int depth = 0, std::string pathRelativeTo = "NULL");
    void dumpNodeSummaryHeader(const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions);
    void dumpNodeSummary(mega::MegaNode* n, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, bool humanreadable = false, const char* title = NULL);
    void dumpTreeSummary(mega::MegaNode* n, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, int recurse, bool show_versions, int depth = 0, bool humanreadable = false, std::string pathRelativeTo = "NULL");
    mega::MegaContactRequest * getPcrByContact(std::string contactEmail);
    bool TestCanWriteOnContainingFolder(std::string *path);
    std::string getDisplayPath(std::string givenPath, mega::MegaNode* n);
    int dumpListOfExported(mega::MegaNode* n, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, std::string givenPath);
    void listnodeshares(mega::MegaNode* n, std::string name);
    void dumpListOfShared(mega::MegaNode* n, std::string givenPath);
    void dumpListOfAllShared(mega::MegaNode* n, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, std::string givenPath);
    void dumpListOfPendingShares(mega::MegaNode* n, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, std::string givenPath);
    std::string getCurrentPath();
    long long getVersionsSize(mega::MegaNode* n);
    void getInfoFromFolder(mega::MegaNode *, mega::MegaApi *, long long *nfiles, long long *nfolders, long long *nversions = NULL);


    //acting
    void loginWithPassword(char *password);
    void changePassword(const char *newpassword, std::string pin2fa = "");
    void actUponGetExtendedAccountDetails(mega::SynchronousRequestListener  *srl, int timeout = -1);
    bool actUponFetchNodes(mega::MegaApi * api, mega::SynchronousRequestListener  *srl, int timeout = -1);
    int actUponLogin(mega::SynchronousRequestListener  *srl, int timeout = -1);
    void actUponLogout(mega::SynchronousRequestListener  *srl, bool deletedSession, int timeout = 0);
    int actUponCreateFolder(mega::SynchronousRequestListener  *srl, int timeout = 0);
    int deleteNode(mega::MegaNode *nodeToDelete, mega::MegaApi* api, int recursive, int force = 0);
    int deleteNodeVersions(mega::MegaNode *nodeToDelete, mega::MegaApi* api, int force = 0);
    void downloadNode(std::string localPath, mega::MegaApi* api, mega::MegaNode *node, bool background, bool ignorequotawar, int clientID, MegaCmdMultiTransferListener *listener = NULL);
    void uploadNode(std::string localPath, mega::MegaApi* api, mega::MegaNode *node, std::string newname, bool background, bool ignorequotawarn, int clientID, MegaCmdMultiTransferListener *multiTransferListener = NULL);
    void exportNode(mega::MegaNode *n, int64_t expireTime, std::string password = std::string(), bool force = false);
    void disableExport(mega::MegaNode *n);
    void shareNode(mega::MegaNode *n, std::string with, int level = mega::MegaShare::ACCESS_READ);
    void disableShare(mega::MegaNode *n, std::string with);
    void createOrModifyBackup(std::string local, std::string remote, std::string speriod, int numBackups);
    std::vector<std::string> listpaths(bool usepcre, std::string askedPath = "", bool discardFiles = false);
    std::vector<std::string> listlocalpathsstartingby(std::string askedPath = "", bool discardFiles = false);
    std::vector<std::string> getlistusers();
    std::vector<std::string> getNodeAttrs(std::string nodePath);
    std::vector<std::string> getUserAttrs();
    std::vector<std::string> getsessions();
    std::vector<std::string> getlistfilesfolders(std::string location);

    void restartsyncs();

    void executecommand(std::vector<std::string> words, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions);

    //doomedtodie
    void syncstat(mega::Sync* sync);
//    const char* treestatename(treestate_t ts);
    bool is_syncable(const char* name);
    int loadfile(std::string* name, std::string* data);
    void signup(std::string name, std::string passwd, std::string email);
    void signupWithPassword(std::string passwd);
    void confirm(std::string passwd, std::string email, std::string link);
    void confirmWithPassword(std::string passwd);

    int makedir(std::string remotepath, bool recursive, mega::MegaNode *parentnode = NULL);
    bool IsFolder(std::string path);
    void doDeleteNode(mega::MegaNode *nodeToDelete, mega::MegaApi* api);

    void confirmDelete();
    void discardDelete();
    void confirmDeleteAll();
    void discardDeleteAll();

    void printTransfersHeader(const unsigned int PATHSIZE, bool printstate=true);
    void printTransfer(mega::MegaTransfer *transfer, const unsigned int PATHSIZE, bool printstate=true);
    void printTransferColumnDisplayer(ColumnDisplayer *cd, mega::MegaTransfer *transfer, bool printstate=true);

#ifdef ENABLE_BACKUPS

    void printBackupHeader(const unsigned int PATHSIZE);
    void printBackupSummary(int tag, const char *localfolder, const char *remoteparentfolder, std::string status, const unsigned int PATHSIZE);
    void printBackupHistory(mega::MegaBackup *backup, const char *timeFormat, mega::MegaNode *parentnode, const unsigned int PATHSIZE);
    void printBackupDetails(mega::MegaBackup *backup, const char *timeFormat);
    void printBackup(int tag, mega::MegaBackup *backup, const char *timeFormat, const unsigned int PATHSIZE, bool extendedinfo = false, bool showhistory = false, mega::MegaNode *parentnode = NULL);
    void printBackup(backup_struct *backupstruct, const char *timeFormat, const unsigned int PATHSIZE, bool extendedinfo = false, bool showhistory = false);
#endif
    void printSyncHeader(const unsigned int PATHSIZE, ColumnDisplayer *cd = nullptr);
    void printSync(int i, std::string key, const char *nodepath, sync_struct * thesync, mega::MegaNode *n, long long nfiles, long long nfolders, const unsigned int PATHSIZE, ColumnDisplayer *cd = nullptr);

    void doFind(mega::MegaNode* nodeBase, const char *timeFormat, std::map<std::string, int> *clflags, std::map<std::string, std::string> *cloptions, std::string word, int printfileinfo, std::string pattern, bool usepcre, mega::m_time_t minTime, mega::m_time_t maxTime, int64_t minSize, int64_t maxSize);

    void move(mega::MegaNode *n, std::string destiny);
    void copyNode(mega::MegaNode *n, std::string destiny, mega::MegaNode *tn, std::string &targetuser, std::string &newname);
    std::string getLPWD();
    bool isValidFolder(std::string destiny);
    bool establishBackup(std::string local, mega::MegaNode *n, int64_t period, std::string periodstring, int numBackups);
    mega::MegaNode *getBaseNode(std::string thepath, std::string &rest, bool *isrelative = NULL);
    void getPathParts(std::string path, std::deque<std::string> *c);

    bool checkAndInformPSA(CmdPetition *inf, bool enforce = false);

    bool checkNoErrors(int errorCode, std::string message = "");
    bool checkNoErrors(mega::MegaError *error, std::string message = "");

    void confirmCancel(const char* confirmlink, const char* pass);
    bool amIPro();

    void processPath(std::string path, bool usepcre, bool &firstone, void (*nodeprocessor)(MegaCmdExecuter *, mega::MegaNode *, bool), MegaCmdExecuter *context = NULL);
    void catFile(mega::MegaNode *n);
    void printInfoFile(mega::MegaNode *n, bool &firstone, int PATHSIZE);


#ifdef HAVE_LIBUV
    void removeWebdavLocation(mega::MegaNode *n, bool firstone, std::string name = std::string());
    void addWebdavLocation(mega::MegaNode *n, bool firstone, std::string name = std::string());
    void removeFtpLocation(mega::MegaNode *n, bool firstone, std::string name = std::string());
    void addFtpLocation(mega::MegaNode *n, bool firstone, std::string name = std::string());
#endif
    bool printUserAttribute(int a, std::string user, bool onlylist = false);
    bool setProxy(const std::string &url, const std::string &username, const std::string &password, int proxyType);
    void fetchNodes(mega::MegaApi *api = nullptr, int clientID = -27);
};

}//end namespace
#endif // MEGACMDEXECUTER_H
