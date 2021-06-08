

#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

// put request updates the information of the user

// request : email

// response : email (string)
//          : balance (int)

// initial balance is 0 when email is entered

AccountService::AccountService() : HttpService("/users")
{
}
// Fetches the user object for this account. You can only fetch the user object for a the account that you authenticated.
void AccountService::get(HTTPRequest *request, HTTPResponse *response)
{

    // get the auth_token
    string auth_token = request->getAuthToken();
    // get User_ID
    string user_id = request->getPathComponents()[1];

    // check if map is empty and the auth token exists in the database already
    if ((m_db->auth_tokens.empty()) || m_db->auth_tokens.find(auth_token) == m_db->auth_tokens.end())
    {
        // could not find matching authentication token
        response->setStatus(404);
        return;
    }


    // retrieve email
    string email = m_db->auth_tokens[auth_token]->email;

    if (email == "") {
        response->setStatus(400);
        return;
    }

    // retrieve balance
    int balance = m_db->auth_tokens[auth_token]->balance;

    Document document;
    Document::AllocatorType &a = document.GetAllocator();
    Value o;
    o.SetObject();

    o.AddMember("email", email, a);
    o.AddMember("balance", balance, a);

    // now some rapidjson boilerplate for converting the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}

// Updates the information for a user.
void AccountService::put(HTTPRequest *request, HTTPResponse *response)
{
    // retrieve the email of user
    string email = (request->formEncodedBody()).get("email");
    string auth_token = request->getAuthToken();

    // checks if map is empty and there exists an authentication token in the database
    if ((m_db->auth_tokens.empty()) || m_db->auth_tokens.find(auth_token) == m_db->auth_tokens.end())
    {
        // could not find matching authentication token
        response->setStatus(404);
        return;
    }
    // update the email in the database
    m_db->auth_tokens[auth_token]->email = email;

    // retrieve the balance
    int balance = m_db->auth_tokens[auth_token]->balance;

    Document document;
    Document::AllocatorType &a = document.GetAllocator();
    Value o;
    o.SetObject();
    response->setStatus(200);
    o.AddMember("email", email, a);
    o.AddMember("balance", balance, a);

    // now some rapidjson boilerplate for converting the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}
