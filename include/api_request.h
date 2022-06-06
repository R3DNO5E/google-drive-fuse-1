
#ifndef GDRIVE_TEST_API_REQUEST_H
#define GDRIVE_TEST_API_REQUEST_H

#include <vector>
#include <string>

class ApiRequest {
private:
    class impl;

    impl *pImpl;
public:
    enum HttpMethod {
        GET, POST
    };
    enum PostDataType {
        URLENCODED, MULTIPART
    };

    ApiRequest(const std::string &base_uri, const std::string &access_token, enum HttpMethod http_method = GET,
               enum PostDataType post_data_type = URLENCODED);

    ~ApiRequest();

    void addHeader(const std::string &data);

    void addQuery(const std::string &key, const std::string &value);

    void addBase64Query(const std::string &key, const std::vector<unsigned char> &data);

    void addPostBody(const std::string &key, const std::string &value);

    std::string perform();
};

#endif //GDRIVE_TEST_API_REQUEST_H
