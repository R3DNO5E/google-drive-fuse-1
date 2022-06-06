#include <json-c/json.h>
#include <vector>
#include <string>
#include "api_request.h"

#define API_FILE_LIST "https://www.googleapis.com/drive/v3/files"
#define API_FILE_GET "https://www.googleapis.com/drive/v3/files"

class GDrive {
private:
    class Node {
    private:
        std::string id;
        std::string name;
    protected:
        const char *token;
    public:
        explicit Node(const char *access_token, std::string &nodeId, std::string &nodeName) {
            token = access_token;
            id = nodeId;
            name = nodeName;
        }

        std::string getId() {
            return id;
        }

        std::string getName() {
            return name;
        }
    };

    class Folder;

    class File;

    class Folder : public Node {
    private:
        std::vector<Folder *> childFolders;
        std::vector<File *> childFiles;
        bool populated = false;

        void populateChild() {
            if (populated) return;
            populated = true;
            ApiRequest apiRequest(API_FILE_LIST, token);
            std::string q = std::string() + "\'" + getId() + "\' in parents";
            apiRequest.addQuery("q", q);
            apiRequest.addQuery("fields","files/name,files/id,files/size,files/mimeType");
            auto res = apiRequest.perform();
            auto *obj_root = json_tokener_parse(res.c_str());
            auto *files = json_object_object_get(obj_root, "files");
            if (files != nullptr) {
                size_t files_count = json_object_array_length(files);
                for (size_t i = 0; i < files_count; i++) {
                    auto *file = json_object_array_get_idx(files, i);
                    auto *mime = json_object_get_string(json_object_object_get(file, "mimeType"));
                    auto *name = json_object_get_string(json_object_object_get(file, "name"));
                    auto *id = json_object_get_string(json_object_object_get(file, "id"));
                    auto size = json_object_get_int64(json_object_object_get(file, "size"));
                    if (std::string(mime) == "application/vnd.google-apps.folder") {
                        childFolders.emplace_back(new Folder(token, std::string(id), std::string(name)));
                    } else {
                        auto f =new File(token, std::string(id), std::string(name));
                        f->setSize(size);
                        childFiles.emplace_back(f);
                    }
                }
            }
            while (json_object_put(obj_root) != 1);
        }

    public:
        std::vector<Folder *> getChildFolders() {
            populateChild();
            return childFolders;
        }

        std::vector<File *> getChildFiles() {
            populateChild();
            return childFiles;
        }

        Folder(const char *access_token, std::string nodeId, std::string nodeName) : Node(access_token, nodeId,
                                                                                          nodeName) {

        }
    };

    class File : public Node {
    private:
        std::vector<unsigned char> content;
        size_t size = 0;
        bool populated = false;
        bool meta = false;

        void populateContent() {
            if (populated) {
                return;
            }
            populated = true;

            ApiRequest api(std::string(API_FILE_GET) + "/" + getId(), token);
            api.addQuery("alt", "media");
            auto ret = api.perform();
            for (auto &e: ret) {
                content.push_back(e);
            }
        }

        void populateMeta(){
            if (meta) {
                return;
            }
            meta = true;
            ApiRequest api(std::string(API_FILE_GET) + "/" + getId(), token);
            api.addQuery("fields", "size");
            auto meta = api.perform();
            auto *obj_root = json_tokener_parse(meta.c_str());
            auto *obj_size = json_object_object_get(obj_root, "size");
            if (obj_size != nullptr) {
                size = json_object_get_int64(obj_size);
            }
        }

    public:
        File(const char *access_token, std::string nodeId, std::string nodeName) : Node(access_token, nodeId,
                                                                                        nodeName) {

        }

/*
        unsigned char *getPartialContent() {
            return nullptr;
        }
*/
        std::vector<unsigned char> getContent() {
            populateContent();
            return content;
        }

        size_t getSize() {
            populateMeta();
            return size;
        }

        void setSize(size_t s){
            meta = true;
            size = s;
        }
    };

    Folder *root = nullptr;

    static std::vector<std::string> parsePath(std::string str) {
        bool absolute_path = false;
        if (!str.empty() && str[0] == '/') {
            absolute_path = true;
        }
        str = "/" + str + "/";
        std::vector<std::string> ret;
        for (auto prev = str.cbegin(), cur = str.cbegin() + 1; cur != str.cend() && prev != str.cend(); cur++) {
            if (*cur == '/') {
                if (cur != prev + 1) {
                    ret.push_back(str.substr(prev - str.cbegin() + 1, cur - prev - 1));
                }
                prev = cur;
            }
        }
        return ret;
    }

    std::pair<Folder *, File *> getNode(std::string path) {
        std::pair<Folder *, File *> ret = std::make_pair(root, nullptr);
        auto p = parsePath(path);
        size_t pn = (int) p.size();
        if (pn == 0) {
            return ret;
        }
        for (size_t i = 0; i < pn - 1; i++) {
            auto c = ret.first;
            ret.first = nullptr;
            for (auto &e: c->getChildFolders()) {
                if (e->getName() == p[i]) {
                    ret.first = e;
                    break;
                }
            }
            if (ret.first == nullptr) {
                return ret;
            }
        }
        for (auto &e: ret.first->getChildFolders()) {
            if (e->getName() == p[pn - 1]) {
                ret.first = e;
                return ret;
            }
        }
        for (auto &e: ret.first->getChildFiles()) {
            if (e->getName() == p[pn - 1]) {
                ret.second = e;
                return ret;
            }
        }
        ret.first = nullptr;
        return ret;
    }

public:
    GDrive(const char *access_token) {
        root = new Folder(access_token, "root", "root");
    }

    std::pair<std::vector<std::string>, std::vector<std::string>> readdir(const std::string &path) {
        std::pair<std::vector<std::string>, std::vector<std::string>> ret;
        auto node = getNode(path);
        if (node.first != nullptr && node.second == nullptr) {
            for (auto &e: node.first->getChildFolders()) {
                ret.first.push_back(e->getName());
            }
            for (auto &e: node.first->getChildFiles()) {
                ret.second.push_back(e->getName());
            }
        }
        return ret;
    }

    bool isdir(const std::string &path) {
        auto node = getNode(path);
        return node.first != nullptr && node.second == nullptr;
    }

    bool isfile(const std::string &path) {
        auto node = getNode(path);
        return node.second != nullptr;
    }

    size_t getsize(const std::string &path) {
        auto node = getNode(path);
        if (node.second != nullptr) {
            return node.second->getSize();
        }
        return 0;
    }

    std::vector<unsigned char> getFile(const std::string &path) {
        auto node = getNode(path);
        if (node.second != nullptr) {
            return node.second->getContent();
        }
        return {};
    }
};