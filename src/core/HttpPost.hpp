#pragma once
#include "core/Backstage.hpp"
#include <string>
#include <utility>
#include <vector>

namespace SatyrAV{

// (2.0.2) One blocking HTTP POST, for the backstage ticker. Called only from
// BackstageSender's worker thread, never from the frame loop.
//
// Windows uses WinHTTP, so https, certificates and the system proxy work
// with nothing extra to install. Other platforms use a plain socket and
// support http only — enough for the Linux dev build and the tests; https
// fails there with a clear error.

struct HttpResult{
	bool sent = false;      // a response came back (any status)
	int status = 0;         // HTTP status code, when sent
	std::string error;      // why it failed, when !sent
	int elapsedMs = 0;
	bool Ok() const{ return sent && status >= 200 && status < 300; }
};

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

HttpResult HttpPost(const Backstage::Url& url, const std::string& body,
	const HttpHeaders& headers, int timeoutMs);

} // namespace SatyrAV
