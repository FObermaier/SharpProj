#include "pch.h"
#include "ProjContext.h"

using namespace SharpProj;
using namespace System::IO;
using System::Net::WebRequest;
using System::Net::WebResponse;
using System::Net::HttpWebRequest;
using System::Net::HttpWebResponse;
using System::Net::HttpStatusCode;

struct my_network_data
{
	gcroot<ProjContext^> ctx;
	gcroot<String^> url;
	gcroot<System::Net::WebResponse^> rp;
	void* chain;
};


static PROJ_NETWORK_HANDLE* my_network_open(
	PJ_CONTEXT* ctx,
	const char* url,
	unsigned long long offset,
	size_t size_to_read,
	void* buffer,
	size_t* out_size_read,
	size_t error_string_max_size,
	char* out_error_string,
	void* user_data)
{
	gcroot<WeakReference<ProjContext^>^>& ref = *(gcroot<WeakReference<ProjContext^>^>*)user_data;
	ProjContext^ pc;
	if (!ref->TryGetTarget(pc))
	{
		strncpy_s(out_error_string, error_string_max_size, "Already disposed", error_string_max_size);
		return nullptr;
	}

	if (error_string_max_size > 0 && out_error_string)
		out_error_string[0] = '\0';


	String^ sUrl = gcnew String(url);
	WebRequest^ rq = WebRequest::Create(sUrl);
	HttpWebRequest^ hrq = dynamic_cast<HttpWebRequest^>(rq);

	if (hrq != nullptr)
	{
		hrq->UserAgent = "System.Net/SharpProj using PROJ " PROJ_VERSION;
		hrq->AddRange((long long)offset, (long long)(offset + size_to_read));
	}
	else
		rq->Headers->Add(String::Format("Range: bytes={0}-{1}", offset, offset + size_to_read - 1));

	WebResponse^ rp;
	bool in_error = true;

	try
	{
		rp = rq->GetResponse();
		in_error = false;
	}
	catch (System::Net::WebException^ wx)
	{
		pc->OnLogMessage(ProjLogLevel::Debug, wx->ToString());
		rp = wx->Response;
	}
	catch (Exception^ ex)
	{
		pc->OnLogMessage(ProjLogLevel::Debug, ex->ToString());
		rp = nullptr;
	}

	HttpWebResponse^ hrp = dynamic_cast<HttpWebResponse^>(rp);

	if (hrp && hrp->StatusCode == HttpStatusCode::PartialContent)
	{
		System::IO::Stream^ s = hrp->GetResponseStream();
		array<unsigned char>^ buf = gcnew array<unsigned char>(size_to_read);

		int r = s->Read(buf, 0, size_to_read);

		while (r > 0 && r < size_to_read)
		{
			int n = s->Read(buf, r, size_to_read - r);

			if (n > 0)
				r += n;
			else
				break;
		}

		if (r > 0)
		{
			pin_ptr<unsigned char> pBuf = &buf[0];
			memcpy(buffer, pBuf, r);
			*out_size_read = r;
		}
		else
		{
			strncpy_s(out_error_string, error_string_max_size, "Read error", error_string_max_size);
			*out_size_read = 0;
			return nullptr;
		}

	}
	else if (!in_error)
	{
		strncpy_s(out_error_string, error_string_max_size, "No partial web response", error_string_max_size);
		return nullptr;
	}
	else
	{
		strncpy_s(out_error_string, error_string_max_size, "Http error", error_string_max_size);
		return nullptr;
	}

	my_network_data* d = new my_network_data();
	d->ctx = pc;
	d->url = sUrl;
	d->rp = rp;
	d->chain = nullptr;
	return (PROJ_NETWORK_HANDLE*)(void*)d;
}

static void my_network_close(
	PJ_CONTEXT* ctx,
	PROJ_NETWORK_HANDLE* handle,
	void* user_data)
{
	my_network_data* d = (my_network_data*)handle;

	d->ctx->free_chain(d->chain);

	delete d;
}

const char* my_network_get_header_value(
	PJ_CONTEXT* ctx,
	PROJ_NETWORK_HANDLE* handle,
	const char* header_name,
	void* user_data)
{
	my_network_data* d = (my_network_data*)handle;

	String^ h = d->rp->Headers->Get(gcnew String(header_name));

	if (h)
	{
		return d->ctx->utf8_chain(h, d->chain);
	}

	return nullptr;
}

size_t my_network_read_range(
	PJ_CONTEXT* ctx,
	PROJ_NETWORK_HANDLE* handle,
	unsigned long long offset,
	size_t size_to_read,
	void* buffer,
	size_t error_string_max_size,
	char* out_error_string,
	void* user_data)
{
	my_network_data* d = (my_network_data*)handle;

	if (error_string_max_size > 0 && out_error_string)
		out_error_string[0] = '\0';

	delete d->rp;
	d->rp = nullptr;

	WebRequest^ rq = WebRequest::Create(d->url);
	HttpWebRequest^ hrq = dynamic_cast<HttpWebRequest^>(rq);

	if (hrq != nullptr)
	{
		hrq->AddRange((long long)offset, (long long)(offset + size_to_read));
	}
	else
		rq->Headers->Add(String::Format("Range: bytes={0}-{1}", offset, offset + size_to_read - 1));

	WebResponse^ rp;
	bool in_error = true;

	try
	{
		rp = rq->GetResponse();
		in_error = false;
	}
	catch (System::Net::WebException^ wx)
	{
		d->ctx->OnLogMessage(ProjLogLevel::Debug, wx->ToString());
		rp = wx->Response;
	}
	catch (Exception^ ex)
	{
		d->ctx->OnLogMessage(ProjLogLevel::Debug, ex->ToString());
		rp = nullptr;
	}

	HttpWebResponse^ hrp = dynamic_cast<HttpWebResponse^>(rp);

	if (hrp && hrp->StatusCode == HttpStatusCode::PartialContent)
	{
		d->rp = rp;
		System::IO::Stream^ s = hrp->GetResponseStream();
		array<unsigned char>^ buf = gcnew array<unsigned char>(size_to_read);

		int r = s->Read(buf, 0, size_to_read);

		while (r > 0 && r < size_to_read)
		{
			int n = s->Read(buf, r, size_to_read - r);

			if (n > 0)
				r += n;
			else
				break;
		}

		if (r > 0)
		{
			pin_ptr<unsigned char> pBuf = &buf[0];
			memcpy(buffer, pBuf, r);
			return r;
		}
		else
		{
			strncpy_s(out_error_string, error_string_max_size, "Read error", error_string_max_size);
			return 0;
		}
	}
	else if (!in_error)
	{
		strncpy_s(out_error_string, error_string_max_size, "No partial web response", error_string_max_size);
		return 0;
	}
	else
	{
		strncpy_s(out_error_string, error_string_max_size, "Http error", error_string_max_size);
		return 0;
	}
	return 0;
}

void ProjContext::SetupNetworkHandling()
{
	proj_context_set_network_callbacks(
		m_ctx,
		my_network_open,
		my_network_close,
		my_network_get_header_value,
		my_network_read_range,
		m_ref);
}
