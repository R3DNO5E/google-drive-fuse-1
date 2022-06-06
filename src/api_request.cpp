#include <curl/curl.h>
#include <vector>
#include <string>
#include <queue>

#include "api_request.h"

class ApiRequest::impl {
private:
    class Encoding {
    public:
        static std::string base64_encode(const unsigned char *data, size_t n, bool url = false);

        static std::string base64_encode(const std::vector<unsigned char> &data, bool url = false);

        static std::string url_encode(const unsigned char *data, size_t n);

        static std::string url_encode(std::string data);

        static std::string querystring_build(const std::vector<std::pair<std::string, std::string>> &q);
    };

    CURL *curl;
    std::vector<std::pair<std::string, std::string>> query, post_data_urlencoded;
    std::string uri, result;
    enum HttpMethod method;
    enum PostDataType post_type;
    curl_mime *mime;
    struct curl_slist *header;

    static size_t curlWriteCallback(char *data, size_t size, size_t count, void *userdata);

public:
    impl(const std::string &base_uri, const std::string &access_token, enum HttpMethod http_method = GET,
         enum PostDataType post_data_type = URLENCODED);

    ~impl();

    void addHeader(const std::string &data);

    void addQuery(const std::string &key, const std::string &value);

    void addBase64Query(const std::string &key, const std::vector<unsigned char> &data);

    void addPostBody(const std::string &key, const std::string &value);

    std::string perform();
};

ApiRequest::ApiRequest(const std::string &base_uri, const std::string &access_token, ApiRequest::HttpMethod http_method,
                       ApiRequest::PostDataType post_data_type) {
    pImpl = new impl(base_uri, access_token, http_method, post_data_type);
}

ApiRequest::~ApiRequest() {
    delete pImpl;
}

void ApiRequest::addHeader(const std::string &data) {
    pImpl->addHeader(data);
}

void ApiRequest::addQuery(const std::string &key, const std::string &value) {
    pImpl->addQuery(key, value);
}

void ApiRequest::addBase64Query(const std::string &key, const std::vector<unsigned char> &data) {
    pImpl->addBase64Query(key, data);
}

void ApiRequest::addPostBody(const std::string &key, const std::string &value) {
    pImpl->addPostBody(key, value);
}

std::string ApiRequest::perform() {
    return pImpl->perform();
}

std::string ApiRequest::impl::Encoding::base64_encode(const unsigned char *data, size_t n, bool url) {
    std::string base64_chars;
    for (char i = 'A'; i <= 'Z'; i++) base64_chars.push_back(i);
    for (char i = 'a'; i <= 'z'; i++) base64_chars.push_back(i);
    for (char i = '0'; i <= '9'; i++) base64_chars.push_back(i);
    base64_chars.append("+/");
    std::queue<unsigned char> q;
    for (size_t i = 0; i < n; i++) {
        q.push(data[i]);
    }
    int bits_read = 0;
    std::string ret;
    while (!q.empty()) {
        unsigned char c = q.front();
        auto t = (unsigned char) (((c << bits_read) & 0b11111100) >> 2);
        if (bits_read != 0) {
            q.pop();
            if (q.empty()) c = 0;
            else c = q.front();
        }
        bits_read = (bits_read + 6) % 8;
        if (bits_read != 6) {
            t = (unsigned char) (t | (c >> (8 - bits_read)));
        }
        ret.push_back(base64_chars[t]);
    }
    if (url) {
        for (auto &e: ret) {
            if (e == '+') e = '-';
            if (e == '/') e = '_';
        }
    } else {
        for (auto i = (4 - (ret.size() % 4)) % 4; i > 0; i--) {
            ret.push_back('=');
        }
    }
    return ret;
}

std::string ApiRequest::impl::Encoding::base64_encode(const std::vector<unsigned char> &data, bool url) {
    size_t n = data.size();
    auto t = new unsigned char[n];
    for (size_t i = 0; i < n; i++) {
        t[i] = data[i];
    }
    auto r = base64_encode(t, n, url);
    delete[] t;
    return r;
}

std::string ApiRequest::impl::Encoding::url_encode(const unsigned char *data, size_t n) {
    std::string ret;
    for (size_t i = 0; i < n; i++) {
        auto t = (unsigned char) data[i];
        if (('a' <= t && t <= 'z') || ('A' <= t && t <= 'Z') || ('0' <= t && t <= '9') || t == '-' ||
            t == '_' ||
            t == '.' || t == '~') {
            ret.push_back((char) t);
        } else {
            char buf[10] = {0};
            sprintf(buf, "%%%x", t);
            ret.append(buf);
        }
    }
    return ret;
}

std::string ApiRequest::impl::Encoding::url_encode(std::string data) {
    size_t n = data.size();
    auto t = new unsigned char[n];
    for (size_t i = 0; i < n; i++) {
        t[i] = data[i];
    }
    auto r = url_encode(t, n);
    delete[]t;
    return r;
}

std::string ApiRequest::impl::Encoding::querystring_build(const std::vector<std::pair<std::string, std::string>> &q) {
    std::string ret;
    for (auto &e: q) {
        ret.append(Encoding::url_encode(e.first));
        ret.push_back('=');
        ret.append(Encoding::url_encode(e.second));
        ret.push_back('&');
    }
    if (!ret.empty()) {
        ret.pop_back();
    }
    return ret;
}


ApiRequest::impl::impl(const std::string &base_uri, const std::string &access_token, enum HttpMethod http_method,
                       enum PostDataType post_data_type) {
    header = nullptr;
    uri = base_uri;
    method = http_method;
    post_type = post_data_type;
    addHeader("Authorization: Bearer " + access_token);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    if (method == POST && post_type == MULTIPART) {
        mime = curl_mime_init(curl);
    }
}

ApiRequest::impl::~impl() {
    curl_easy_cleanup(curl);
    curl_slist_free_all(header);
    if (method == POST && post_type == MULTIPART) {
        curl_mime_free(mime);
    }
}

void ApiRequest::impl::addQuery(const std::string &key, const std::string &value) {
    query.emplace_back(key, value);
}

void ApiRequest::impl::addBase64Query(const std::string &key, const std::vector<unsigned char> &data) {
    query.emplace_back(key, Encoding::base64_encode(data, true));
}

void ApiRequest::impl::addHeader(const std::string &data) {
    header = curl_slist_append(header, data.c_str());
}

void ApiRequest::impl::addPostBody(const std::string &key, const std::string &value) {
    if (post_type == URLENCODED) {
        post_data_urlencoded.emplace_back(key, value);
    } else if (post_type == MULTIPART) {
        curl_mimepart *part = curl_mime_addpart(mime);
        curl_mime_data(part, value.c_str(), value.size());
        curl_mime_name(part, key.c_str());
    }
}

size_t ApiRequest::impl::curlWriteCallback(char *data, size_t size, size_t count, void *userdata) {
    auto result = (std::string *) userdata;
    result->append(data, size * count);
    return size * count;
}

std::string ApiRequest::impl::perform() {
    if (method == POST) {
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        if (post_type == URLENCODED) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Encoding::querystring_build(post_data_urlencoded).c_str());
        } else if (post_type == MULTIPART) {
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        }
    }
    curl_easy_setopt(curl, CURLOPT_URL, (uri + "?" + Encoding::querystring_build(query)).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    curl_easy_perform(curl);
    return result;
}
