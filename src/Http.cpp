#include "Http.hpp"
#include "Logging.hpp"

#include "System/Net/Http/HttpClient.hpp"
#include "System/Net/Http/HttpRequestMessage.hpp"
#include "System/Net/Http/HttpResponseMessage.hpp"
#include "System/Net/Http/HttpContent.hpp"
#include "System/Net/Http/StringContent.hpp"
#include "System/Net/Http/HttpMethod.hpp"
#include "System/Net/Http/HttpCompletionOption.hpp"
#include "System/Net/Http/Headers/HttpRequestHeaders.hpp"
#include "System/Net/Http/Headers/AuthenticationHeaderValue.hpp"
#include "System/Threading/Tasks/Task.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Text/Encoding.hpp"
#include "System/TimeSpan.hpp"
#include "System/Uri.hpp"

// SafePtr's "don't wrap Unity types" static_assert instantiates
// is_assignable_v<UnityEngine::Object, T>, which needs Object to be a
// complete type -- cordl only forward-declares it otherwise.
#include "UnityEngine/Object.hpp"

#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"

namespace {

using namespace System::Net::Http;
using SendTask = System::Threading::Tasks::Task_1<HttpResponseMessage*>;
using ReadTask = System::Threading::Tasks::Task_1<StringW>;

// Yielding a null IEnumerator from a Unity coroutine means "resume next
// frame", which is exactly the poll cadence we want while a Task runs.
constexpr System::Collections::IEnumerator* kNextFrame = nullptr;

std::string ToStd(StringW s) {
    if (!s) return {};
    return static_cast<std::string>(s);
}

}  // namespace

namespace YouTubeLiveChat::Http {

custom_types::Helpers::Coroutine Request(std::string method,
                                         std::string url,
                                         std::string body,
                                         std::string contentType,
                                         std::string bearerToken,
                                         Callback callback) {
    Response result{};

    // The C# objects below outlive a co_yield, at which point the only
    // reference to them is this coroutine's heap frame -- which the il2cpp GC
    // does not scan. SafePtr registers them as GC roots for as long as the
    // frame is alive, which is what keeps them from being collected mid-request.
    SafePtr<HttpClient> client;
    SafePtr<SendTask> sendTask;
    SafePtr<ReadTask> readTask;

    // Every il2cpp call is guarded: a managed exception escaping into the
    // coroutine frame would kill the whole poll loop rather than just failing
    // this one request.
    try {
        client = HttpClient::New_ctor();
        client->set_Timeout(System::TimeSpan::FromSeconds(20.0));

        auto* request = HttpRequestMessage::New_ctor(
            method == "POST" ? HttpMethod::get_Post() : HttpMethod::get_Get(),
            System::Uri::New_ctor(StringW(url)));

        if (method == "POST") {
            request->set_Content(StringContent::New_ctor(
                StringW(body), System::Text::Encoding::get_UTF8(), StringW(contentType)));
        }
        if (!bearerToken.empty()) {
            request->get_Headers()->set_Authorization(
                Headers::AuthenticationHeaderValue::New_ctor(StringW("Bearer"), StringW(bearerToken)));
        }

        sendTask = client->SendAsync(request, HttpCompletionOption::ResponseContentRead);
    } catch (std::exception const& e) {
        result.error = std::string("request setup failed: ") + e.what();
        Log().error("HTTP setup failed: {}", e.what());
        callback(result);
        co_return;
    }

    while (!sendTask->get_IsCompleted()) {
        co_yield kNextFrame;
    }

    try {
        if (sendTask->get_IsFaulted() || sendTask->get_IsCanceled()) {
            result.error = "network error";
            Log().warn("HTTP transport failure");
            callback(result);
            co_return;
        }

        auto* response = sendTask->get_Result();
        result.completed = true;
        result.status = static_cast<long>(response->get_StatusCode().value__);
        result.ok = response->get_IsSuccessStatusCode();
        readTask = response->get_Content()->ReadAsStringAsync();
    } catch (std::exception const& e) {
        result.completed = false;
        result.ok = false;
        result.error = e.what();
        Log().warn("HTTP response handling failed: {}", e.what());
        callback(result);
        co_return;
    }

    while (!readTask->get_IsCompleted()) {
        co_yield kNextFrame;
    }

    try {
        if (!readTask->get_IsFaulted() && !readTask->get_IsCanceled()) {
            result.body = ToStd(readTask->get_Result());
        }
        client->Dispose(true);
    } catch (std::exception const& e) {
        Log().warn("Reading HTTP body failed: {}", e.what());
    }

    callback(result);
    co_return;
}

}  // namespace YouTubeLiveChat::Http
