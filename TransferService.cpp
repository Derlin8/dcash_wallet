#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "TransferService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

TransferService::TransferService() : HttpService("/transfers") {}

// Transfers money from your account to another user.
// You must make sure that there aren't any negative balances and that the to username exists for this API call to succeed.

// Arguments
// To : The username of the recipient
// Amount : The amount to transfer in cents

// Response
// Balance : (Int) The new balance on the account in cents
// Transfers : (Array) A transfer history for this user

// Transfer Object
// From : (String) the username of the user who sent the transfer
// To : (String) the username of the user who recv the transfer
// Amount : (Int) The amount of the transfer in cents

void TransferService::post(HTTPRequest *request, HTTPResponse *response)
{
    string auth_token = request->getAuthToken();
    // get the 'to' parameter
    string to = (request->formEncodedBody()).get("to");
    // get the 'amount' parameter
    string amount_token = (request->formEncodedBody()).get("amount");
    // convert string to int
    int amount = stoi(amount_token);

    // If there is no auth token argument
    if (request->getAuthToken() == "")
    {
        response->setStatus(401);
        return;
    }
    // If there is no "to" or "amount" arguments
    if (to == "" || amount_token == "")
    {
        response->setStatus(400);
        return;
    }
    // if the amount transferred is less than 50
    if (amount < 50)
    {
        response->setStatus(401);
        return;
    }

    // use rapidjson to create a return object
    Document document;
    Document::AllocatorType &a = document.GetAllocator();
    Value o;
    o.SetObject();

    User *to_user = m_db->users[to];
    User *from_user = m_db->auth_tokens[auth_token];

    // Make sure to_user and from_user exists
    if (to_user == NULL || from_user == NULL)
    {
        response->setStatus(400);
        return;
    }

    // Make sure the user can make the transfer
    // If the amount exceeds the from_user's current balance, they cannot send
    if (amount > from_user->balance)
    {
        response->setStatus(400);
        return;
    }

    to_user->balance += amount;
    from_user->balance -= amount;

    Transfer *current_transfer = new Transfer();
    current_transfer->from = from_user;
    current_transfer->to = to_user;
    current_transfer->amount = amount;

    m_db->transfers.push_back(current_transfer);
    Value transfer_history;
    transfer_history.SetArray();

    for (unsigned long int i = 0; i < m_db->transfers.size(); i++)
    {
        if (m_db->transfers[i]->from->username.compare(from_user->username) == 0 ||
            m_db->transfers[i]->to->username.compare(from_user->username) == 0)
        {
            Value t;
            t.SetObject();
            t.AddMember("from", m_db->transfers[i]->from->username, a);
            t.AddMember("to", m_db->transfers[i]->to->username, a);
            t.AddMember("amount", m_db->transfers[i]->amount, a);
            transfer_history.PushBack(t, a);
        }
    }

    // create JSON response object
    o.AddMember("balance", from_user->balance, a);
    o.AddMember("transfers", transfer_history, a);

    // now some rapidjson boilerplate for converting the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}
