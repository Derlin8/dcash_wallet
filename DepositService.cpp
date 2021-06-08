

#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

DepositService::DepositService() : HttpService("/deposits") {}

// Deposits
// amount : The amount to charge, in cents (must be >= 50)
// stripe_token : The Stripe token for the card you want to charge

// Response Object
// balance : (Int) The new balance on the account in cents
// deposits : (Array) All of the deposit object for this user

// Deposit Object
// to : (String) the username for the user who deposited
// amount : (Int) the amount of the deposit in cents
// stripe_charge_id	: (String) the Stripe charge ID for the deposit

void DepositService::post(HTTPRequest *request, HTTPResponse *response)
{
    // Make sure amount is >= 50 X
    // Make sure the stripe token is an integer
    // Make sure the auth token is related to a user
    // get the auth token
    string auth_token = request->getAuthToken();
    // get the 'amount' parameter
    string amount_string = (request->formEncodedBody()).get("amount");
    // get the 'stripe_token' parameter
    string stripe_token_str = (request->formEncodedBody()).get("stripe_token");
    // convert string to int
    int amount = stoi(amount_string);

    if (amount < 50)
    {
        // amount needs to be >= 50 deposited
        response->setStatus(400);
        return;
    }
    // check if the map is empty first
    if ((m_db->auth_tokens.empty()) || m_db->auth_tokens.find(auth_token) == m_db->auth_tokens.end())
    {

        // could not find matching authentication token
        response->setStatus(404);
        return;
    }

    User *user = m_db->auth_tokens[auth_token];

    // Make API call to Stripe to make sure credit card is authorized
    // If not authorized, return error 403

    WwwFormEncodedDict body;
    body.set("amount", amount);
    body.set("currency", "usd");
    body.set("source", stripe_token_str);
    body.set("description", "gunrock_auth");
    string encoded_body = body.encode();

    // from the gunrock server to Stripe
    HttpClient client("api.stripe.com", 443, true);
    client.set_basic_auth(m_db->stripe_secret_key, "");
    HTTPClientResponse *client_response = client.post("/v1/charges", encoded_body);
    Document *d = client_response->jsonBody();
    bool has_id = (*d).HasMember("id");
    if (!has_id)
    {
        response->setStatus(403);
        delete d;
        return;
    }
    string stripe_charge_id = (*d)["id"].GetString();
    delete d;

    user->balance += amount;

    Deposit *current_deposit = new Deposit();
    current_deposit->to = user;
    current_deposit->amount = amount;
    current_deposit->stripe_charge_id = stripe_charge_id; // stripe token
    m_db->deposits.push_back(current_deposit);

    // JSON response
    Document document;
    Document::AllocatorType &a = document.GetAllocator();

    Value deposit_history;
    deposit_history.SetArray();

    vector<Deposit *>::iterator it;
    for (it = m_db->deposits.begin(); it != m_db->deposits.end(); it++)
    {

        Deposit *ptr = *it;

        if (ptr->to->username.compare(user->username) == 0)
        {
            Value d;
            d.SetObject();
            d.AddMember("to", ptr->to->username, a);
            d.AddMember("amount", ptr->amount, a);
            d.AddMember("stripe_charge_id", ptr->stripe_charge_id, a);
            deposit_history.PushBack(d, a);
        }
    }

    Value o;
    o.SetObject();

    // create JSON response object
    o.AddMember("balance", user->balance, a);
    o.AddMember("deposits", deposit_history, a);

    // now some rapidjson boilerplate for converting the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}
