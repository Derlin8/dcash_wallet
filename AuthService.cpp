
#define RAPIDJSON_HAS_STDSTRING 1
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AuthService.h"
#include "StringUtils.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AuthService::AuthService() : HttpService("/auth-tokens")
{
}

void AuthService::post(HTTPRequest *request, HTTPResponse *response)
{
    // first step is to parse the request
    // 3 cases, if the user enters a user name that is in the system, if it is not in the system, if the password matches
    // creates a new auth-token when user logs in
    string username = (request->formEncodedBody()).get("username");
    string password = (request->formEncodedBody()).get("password");

    string auth_token = StringUtils::createAuthToken();

    // check if the username is uppercase
    for (unsigned long int i = 0; i < username.size(); i++) {
        if (isupper(username[i])) {
            response->setStatus(400);
            return;
        }
    }

    if (m_db->users.count(username) == 0)
    {
        // stores information for users
        string user_ID = StringUtils::createUserId();
        User *newUser = new User;
        newUser->username = username;
        newUser->password = password;
        newUser->user_id = user_ID;
        newUser->balance = 0;
        m_db->users.insert(make_pair(username, newUser));
        // stores information for authorization tokens
        m_db->auth_tokens.insert(make_pair(auth_token, newUser));
        

        // successfully logged in and create a response object
        Document document;
        Document::AllocatorType &a = document.GetAllocator();
        Value o;
        o.SetObject();

        o.AddMember("auth_token", auth_token, a);
        o.AddMember("user_id", user_ID, a);

        // now some rapidjson boilerplate for converting the JSON object to a string
        document.Swap(o);
        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        document.Accept(writer);

        // set the return object
        response->setContentType("application/json");
        response->setBody(buffer.GetString() + string("\n"));
        response->setStatus(201);
    }
    else if (m_db->users.count(username) > 0)
    {
        // if the passwords do not match
        if (m_db->users[username]->password.compare(password) != 0)
        {
            // username matches but not password
            response->setStatus(403);
        }
        else
        {
            Document document;
            Document::AllocatorType &a = document.GetAllocator();
            Value o;
            o.SetObject();

            o.AddMember("auth_token", auth_token, a);
            o.AddMember("user_id", m_db->users[username]->user_id, a);

            // now some rapidjson boilerplate for converting the JSON object to a string
            document.Swap(o);
            StringBuffer buffer;
            PrettyWriter<StringBuffer> writer(buffer);
            document.Accept(writer);

            // set the return object
            response->setContentType("application/json");
            response->setBody(buffer.GetString() + string("\n"));
            m_db->auth_tokens.insert(make_pair(auth_token, m_db->users[username]));
            // success and a new resource was generated
            response->setStatus(200);
        }
    }
}
void AuthService::del(HTTPRequest *request, HTTPResponse *response)
{
    // checks if the map is empty and the auth token from the request matches the auth token in the database 
    if ((m_db->auth_tokens.empty()) || m_db->auth_tokens.find(request->getAuthToken()) == m_db->auth_tokens.end())
    {
        // could not find matching authentication token
        response->setStatus(404);
        return;
    }
    // access the user from auth token
    string auth_token = request->getAuthToken();
    User *user = m_db->auth_tokens[auth_token];
    
    // erases the user from the username map
    m_db->users.erase(user->username);
    // erases the user from the auth_token map
    m_db->auth_tokens.erase(auth_token);
    response->setStatus(200);


}
