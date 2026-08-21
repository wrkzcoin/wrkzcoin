// Forward declarations for the cpp-httplib types named in our headers. Only the
// translation units that actually talk HTTP include the full httplib.h (20k
// lines). Keep the struct/class keywords in sync with httplib.h.
#pragma once

namespace httplib
{
struct Request;
struct Response;
class Server;
class Client;
class Result;
} // namespace httplib
