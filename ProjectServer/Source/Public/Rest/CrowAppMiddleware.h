#pragma once

namespace crow
{
	struct request;
	struct response;
}

/** Crow cpp middleware */
struct FCrowAppMiddleware
{
	struct context {};

	void before_handle(crow::request& Req, crow::response& Res, context& Ctx);
	void after_handle(crow::request& Req, crow::response& Res, context& Ctx);
};
