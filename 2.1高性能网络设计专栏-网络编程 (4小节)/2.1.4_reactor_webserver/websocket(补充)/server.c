

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

#include "reactor.h"

// websocket
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/*
nty_ophdr 是 WebSocket 帧的前 2 个字节结构：
第1字节：FIN(1) + RSV1-3(3) + Opcode(4)
第2字节：Mask(1) + Payload length(7)
*/
struct _nty_ophdr
{
	unsigned char opcode : 4,
		rsv3 : 1,
		rsv2 : 1,
		rsv1 : 1,
		fin : 1;
	unsigned char payload_length : 7,
		mask : 1;

} __attribute__((packed));

// unsigned char data[8] ：使用8个字节表示发送数据的长度
struct _nty_websocket_head_126
{
	unsigned short payload_length; // 对应 len=126 时的 2 字节长度
	char mask_key[4];			   // 4字节
	unsigned char data[8];

} __attribute__((packed));
/*
unsigned char data[8] 并不是websocket协议要求的固定字段
它存放的是 payload（也就是被 mask_key 异或加密后的数据）的前 8 个字节
//////实际上这个字段是作者自定义的
*/

struct _nty_websocket_head_127
{

	unsigned long long payload_length; // 对应 len=127 时的 8 字节长度
	char mask_key[4];

	unsigned char data[8];

} __attribute__((packed));

typedef struct _nty_websocket_head_127 nty_websocket_head_127;
typedef struct _nty_websocket_head_126 nty_websocket_head_126;
typedef struct _nty_ophdr nty_ophdr;

/*
在 WebSocket 握手时，服务器需要生成 Sec-WebSocket-Accept 头，这个过程是：
1-收到客户端发来的 Sec-WebSocket-Key（是一个随机的 Base64 字符串）。
2-在它后面拼上固定 GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"。
3-对拼接后的字符串做 SHA1 哈希（得到 20 字节二进制）。
4-用这个 base64_encode 函数把 20 字节的 SHA1 结果转成 Base64 字符串。 ✔
5-把这个结果作为 Sec-WebSocket-Accept 发送给客户端。
*/
int base64_encode(char *in_str, int in_len, char *out_str)
{
	// 声明两个 OpenSSL BIO 指针：一个用于 base64 过滤器（编码器），一个用于内存缓冲
	BIO *b64, *bio;
	// BUF_MEM 是 OpenSSL 提供的内存缓冲结构，BIO_get_mem_ptr 会返回它的指针，包含内存地址和长度。
	BUF_MEM *bptr = NULL;
	// 用于记录最终长度（返回值）
	size_t size = 0;

	if (in_str == NULL || out_str == NULL)
		return -1;

	// 创建一个 base64 过滤 BIO（用于对写入的数据进行 base64 编码）
	b64 = BIO_new(BIO_f_base64());
	// 创建一个内存 BIO（把流写入内存缓冲区，可通过 BIO_get_mem_ptr 访问）
	bio = BIO_new(BIO_s_mem());
	// 把两个 BIO 链接起来：b64 在上层（top），bio（内存）在下层。BIO_push 返回链的顶端（因此 bio 现在指向 top，即 b64）。之后对 bio 的写入会先走 b64 编码然后写进内存 BIO
	bio = BIO_push(b64, bio);

	// 将输入数据写入 BIO 链（通过 base64 过滤器写入内存）
	BIO_write(bio, in_str, in_len);
	// 刷新 BIO，确保编码器输出所有剩余数据（重要，base64 编码器会在 flush 时输出尾部填充 = ）
	BIO_flush(bio);

	// 获得内存 BIO 的内部 BUF_MEM 指针，bptr->data 指向结果数据，bptr->length 给出长度（注意：bptr->data 不是以 \0 结尾的 C 字符串，除非手动加上终止符）
	BIO_get_mem_ptr(bio, &bptr);
	memcpy(out_str, bptr->data, bptr->length);
	// 试图把最后一个字节替换为 \0 以结束 C 字符串。作者意图可能是去掉 base64 输出末尾的 \n（因为 OpenSSL 的 base64 BIO 默认会插入换行）
	out_str[bptr->length - 1] = '\0';
	size = bptr->length;
	// 释放 BIO 链和内部分配的内存（注意：bptr->data 所指向的内存也将被释放）
	BIO_free_all(bio);
	return size;
}

/*
用途：从 allbuf[level...] 读取到遇到 \r\n 为止，把一行写入 linebuf，返回下一开始位置 index
*/
int readline(char *allbuf, int level, char *linebuf)
{
	int len = strlen(allbuf);

	for (; level < len; ++level)
	{
		if (allbuf[level] == '\r' && allbuf[level + 1] == '\n')
			return level + 2;
		else
			*(linebuf++) = allbuf[level];
	}

	return -1;
}

/*
在 WebSocket 协议（RFC6455）里规定：客户端 → 服务器 的数据帧必须进行Mask（加掩码），服务器 → 客户端 的数据帧不用Mask
Masking 的原理：
* 客户端发消息时，会随机生成一个 4 字节的 masking key
* 每个字节的真实数据（payload）会用 mask 循环异或（^）加密  		A ^ B
* 服务器收到数据后，需要用相同的方式再异或一次把原文还原出来		 A ^ B ^ B = A   ✔
*/
// 用途：对 data[0..len-1] 逐字节按 mask 的 4 字节循环做 XOR，解码客户端发来的 payload
void demask(char *data, int len, char *mask)
{
	int i;
	for (i = 0; i < len; i++)
		*(data + i) ^= *(mask + (i % 4)); // payload[i] ^= mask[i % 4]
}

// 用途：从原始帧 stream 解析出 payload 的起始地址，返回该指针；同时把 payload 解掩码
char *decode_packet(unsigned char *stream, char *mask, int length, int *ret)
{
	/*
	 把 stream 指向的内存当作 nty_ophdr 结构体来解析，hdr 只会解析 stream 的前 2 字节（即 WebSocket 帧头）
	 可以通过hdr->mask 访问帧头的字段（比如掩码）
	 */
	nty_ophdr *hdr = (nty_ophdr *)stream;

	// 接下来处理payload_length（第二个字节的后7位/2字节/8字节）、掩码（4 字节）、 实际数据
	unsigned char *data = stream + sizeof(nty_ophdr); // 指向帧头2字节之后的位置（即 payload len 字段后面的数据）
	int size = 0;
	int start = 0;
	// char mask[4] = {0};
	int i = 0;

	// 按位与，作用是把帧头第二个字节的最高位 MASK 清零，保留低7位长度值。根据这个长度值，分为3种情况：
	//  第一种情况：126：接下来 2 字节表示payload实际长度（16位无符号）
	if ((hdr->mask & 0x7F) == 126)
	{

		// 取出 2字节的 payload_length
		nty_websocket_head_126 *hdr126 = (nty_websocket_head_126 *)data;
		size = hdr126->payload_length;

		// 取 4 字节 mask key
		for (i = 0; i < 4; i++)
		{
			mask[i] = hdr126->mask_key[i];
		}

		// payload 起始位置在第 8 个字节( 2+4 + 2(WebSocket帧的前2个字节))
		start = 8;
	}
	// 第二种情况：127：接下来 8 字节表示payload实际长度（64位无符号）
	else if ((hdr->mask & 0x7F) == 127)
	{

		/// 取出 8字节的 payload_length
		nty_websocket_head_127 *hdr127 = (nty_websocket_head_127 *)data;
		size = hdr127->payload_length;

		// 取 4 字节 mask key
		for (i = 0; i < 4; i++)
		{
			mask[i] = hdr127->mask_key[i];
		}

		// payload 起始位置在第 8 个字节( 8+4 + 2(WebSocket帧的前2个字节))
		start = 14;
	}
	// 第三种情况：0–125：payload长度就是第2字节的低7位（表示的数值）
	else
	{
		size = hdr->payload_length;
		/*
		从数据帧中获得掩码
		数据帧当前位置：帧头之后的2字节，即Payload len之后第三个字节初始位置
		因为没有payload_length，所以后面直接是4个字节的掩码。
		所以直接复制4字节的掩码
		*/
		memcpy(mask, data, 4);
		// payload 起始位置在第 6 个字节( 4 + 2(WebSocket帧的前2个字节))
		start = 6;
	}

	// payload 长度
	*ret = size;
	// 解码（去掩码）
	demask(stream + start, size, mask);
	// 返回值是 payload 的起始地址（已经解码好的数据）
	return stream + start;
}

// 用途：服务端把要发送的 payload（stream/length）封装成一个 WebSocket 帧写入 buffer）给客户端
int encode_packet(char *buffer, char *mask, char *stream, int length)
{

	nty_ophdr head = {0};
	head.fin = 1;
	head.opcode = 1;
	int header_len = 0; // header_len是整个帧头的总长度(不包括实际数据的长度)，包括基础的2字节头部和所有扩展长度 + 掩码字段的长度。

	// WebSocket 帧：2个字节帧头 + （ 扩展头部（扩展长度+掩码）） + 实际数据
	if (length < 126)
	{
		head.payload_length = length;
		memcpy(buffer, &head, sizeof(nty_ophdr));
		header_len = sizeof(nty_ophdr); // 2
	}
	else if (length < 0xffff)
	{
		nty_websocket_head_126 hdr = {0};
		hdr.payload_length = length;
		memcpy(hdr.mask_key, mask, 4);

		memcpy(buffer, &head, sizeof(nty_ophdr));
		memcpy(buffer + sizeof(nty_ophdr), &hdr, sizeof(nty_websocket_head_126));
		header_len = sizeof(nty_ophdr) + sizeof(nty_websocket_head_126);
	}
	else
	{

		nty_websocket_head_127 hdr = {0};
		hdr.payload_length = length;
		memcpy(hdr.mask_key, mask, 4);

		memcpy(buffer, &head, sizeof(nty_ophdr));
		memcpy(buffer + sizeof(nty_ophdr), &hdr, sizeof(nty_websocket_head_127));

		header_len = sizeof(nty_ophdr) + sizeof(nty_websocket_head_127);
	}

	// 注意：参考代码中写的是数据起始位置写的是buffer + 2，这应该是错误的
	// 因为只考虑数据长度<126的情况。而没考虑其他两种情况: 扩展头部（扩展长度+掩码）  2+4（掩码长度） / 8+4
	memcpy(buffer + header_len, stream, length);

	// 返回值是websocket帧的长度（整个帧头的总长度 + 实际数据的长度length）
	return header_len + length;
}

#define WEBSOCK_KEY_LENGTH 19

/*服务端
循环读取 HTTP 请求的每一行（readline），查找包含 Sec-WebSocket-Key 的那一行。
找到后把该 linebuf 与 GUID 拼接，然后对拼接后的字符串进行 SHA1，再 base64_encode 作为 Sec-WebSocket-Accept。
构造 101 Switching Protocols 的响应并写回 c->wbuffer，设置 c->wlength。
*/
int handshake(struct conn *c)
{

	// ev->buffer , ev->length

	char linebuf[1024] = {0};  // 临时存放从接收缓冲区（c->rbuffer）中读取的一行 HTTP 请求头
	int idx = 0;			   // 记录 readline 读取到的位置
	char sec_data[128] = {0};  // 存放 SHA1 后的二进制结果（结果是固定的160bit=20bytes）
	char sec_accept[32] = {0}; // 最终存放 Base64 编码后的字符串，发给客户端
	/*Base64 规则：
	每 3 字节 数据编码成 4 字符; 不足 3 字节时用 = 补齐
	Base64长度=4 × ⌈原始字节数 / 3 ⌉ = 4 × ⌈6.666...⌉= 4 × 7 = 28 bytes
	*/

	do
	{

		memset(linebuf, 0, 1024);
		idx = readline(c->rbuffer, idx, linebuf);

		// 检查 linebuf 字符串中是否包含子串 "Sec-WebSocket-Key"
		if (strstr(linebuf, "Sec-WebSocket-Key"))
		{

			// linebuf: Sec-WebSocket-Key: QWz1vB/77j8J8JcT/qtiLQ==
			strcat(linebuf, GUID);

			// linebuf:
			// Sec-WebSocket-Key: QWz1vB/77j8J8JcT/qtiLQ==258EAFA5-E914-47DA-95CA-C5AB0DC85B11

			SHA1(linebuf + WEBSOCK_KEY_LENGTH, strlen(linebuf + WEBSOCK_KEY_LENGTH), sec_data); // openssl

			base64_encode(sec_data, 20, sec_accept); // 参考代码写成strlen(sec_data)，但SHA1 结果可能包含 \0，用 strlen 会截断。直接用20作为长度

			memset(c->wbuffer, 0, BUFFER_LENGTH);

			c->wlength = sprintf(c->wbuffer, "HTTP/1.1 101 Switching Protocols\r\n"
											 "Upgrade: websocket\r\n"
											 "Connection: Upgrade\r\n"
											 "Sec-WebSocket-Accept: %s\r\n\r\n",
								 sec_accept);
			/*
			按 WebSocket 协议要求格式化握手应答：
				101 Switching Protocols 表示协议升级成功
				Upgrade: websocket 指明升级的协议
				Connection: Upgrade 告诉客户端连接升级
				Sec-WebSocket-Accept：放 Base64 后的字符串
			*/

			printf("ws response : %s\n", c->wbuffer);

			break;
		}

	} while ((c->rbuffer[idx] != '\r' || c->rbuffer[idx + 1] != '\n') && idx != -1);
	/*
	直到遇到空行（\r\n）代表 HTTP 头结束，或者 readline 返回 -1（读取失败）
		如果 开头字符不是 \r 或 下一个字符不是 \n，就继续循环
		即当当前开头字符是 \r 且下一个字符是 \n 时，条件为假 → 循环退出
	eg:
		第一行末尾：...HTTP/1.1\r\n → 开头字符不是 \r（它是 H 或其他内容），所以继续
		第二行末尾：...example.com\r\n → 继续
		遇到空行时：这一行开头就是 \r，下一个字符是 \n → 退出循环
	*/

	return 0;
}

// 服务端（收到客户端发来的数据后构造一个WebSocket数据帧） -> 客户端
int ws_request(struct conn *c)
{

	// 1-如果 status == 0，说明刚建立 TCP 连接，还没有 WebSocket 协议握手
	if (c->status == 0)
	{
		// 调用 handshake(c) 去处理握手请求（解析 HTTP 头，生成 Sec-WebSocket-Accept，返回 101 响应等）
		handshake(c);
		// 握手完成后，将状态切到 1（已进入 WebSocket 数据帧阶段）
		c->status = 1;
	}
	// 2-如果 status == 1，说明握手完成，现在处理 WebSocket 数据帧
	else if (c->status == 1)
	{
		char mask[4] = {0};
		int ret = 0;

		char *data = decode_packet(c->rbuffer, mask, c->rlength, &ret);
		/*
		调用 decode_packet(...)：
			解析 c->rbuffer 里的 WebSocket 帧
			取出掩码
			解码数据（对 payload 进行掩码异或）
			返回解码后的 data，ret 里是数据长度
		*/

		printf("data : %s , length : %d\n", data, ret);

		ret = encode_packet(c->wbuffer, mask, data, ret);
		/*
		调用 encode_packet(...)：
			把 data 再编码成一个 WebSocket 帧
			用同一个 mask（这点在真正的服务器里一般不会这么做，因为服务端发的帧不需要掩码）
			返回编码后的长度
		*/
		c->wlength = ret; // 把长度写到 c->wlength，等待发送
	}

	return 0;
}

/*
这份代码中没写，参考代码：
if (c->status == 2) {
	c->wlength = encode_packet(c->wbuffer, c->mask, c->payload, c->wlength);
	c->status = 1;
}
它是主动发送数据的路径，比如服务端收到了一个业务指令，需要推送数据给客户端。
当 status == 2 时，说明 c->payload 已经准备好了（可能是应用逻辑生成的消息），这时就调用 encode_packet 封装为 WebSocket 帧。
编码完成后 status 设回 1，表示等待下一次消息处理。
*/
int ws_response(struct conn *c)
{
	return 0;
}

/*
客户端			websocket.html（浏览器执行 JS）			主动连接服务端，发送消息，接收并显示响应
服务端			erver.c（Linux 上运行）					监听端口，完成 WebSocket 握手，解码客户端消息并回显

执行流程
1.你先在 Linux 上启动服务端程序
这个程序监听 8888 端口，等待客户端连接。
一旦收到 HTTP 请求，它会检查是不是 WebSocket 升级请求（通过 handshark() 完成握手）。

2.你在浏览器打开 websocket.html
你输入服务器地址（例：192.168.232.132:8888），点击 Connect。
doConnect() 会执行 new WebSocket("ws://192.168.232.132:8888") → 浏览器向服务器发送 WebSocket 握手请求（HTTP 升级）。

3.握手阶段
服务端收到握手 HTTP 请求 → ws_request() 检测到 c->status == 0 → 调用 handshake()。
handshake() 从 HTTP 请求中取出 Sec-WebSocket-Key，加上固定 GUID，做 SHA1 + Base64 生成 Sec-WebSocket-Accept，然后返回 101 Switching Protocols 响应。
浏览器收到 101 响应，连接状态变成 open，ws.onopen 回调执行。

4.发送消息
你在浏览器输入消息（Message），点击 Send。
浏览器会用 WebSocket 协议数据帧（带掩码）发送给服务端。
服务端在 ws_request() 里，检测到 c->status == 1，调用 decode_packet() 解码数据帧（解掩码）。
服务端打印 data : ...，然后用 encode_packet() 打包回一个帧，再写回给客户端。

5.接收消息
浏览器收到服务器的帧 → ws.onmessage 回调触发，显示在日志框里。
*/

/*
浏览器 websocket.html (客户端)             Linux server.c (服务端)
────────────────────────────────────────────────────────────────────────
点击 Connect
│ doConnect(addr)
│   └── new WebSocket("ws://addr")  ─────►  [等待 TCP 连接 + HTTP 请求接收]
│                                         tcp_accept()
│                                         ws_request(c)   // c->status == 0
│                                           └── handshark(c)
│                                                 ├─ readline()       // 读取每行HTTP请求
│                                                 ├─ SHA1()           // 拼 GUID 后做 SHA1
│                                                 ├─ base64_encode()  // 转成 Sec-WebSocket-Accept
│                                                 └─ sprintf()        // 生成 101 响应
│                                         ◄─────── 返回 "101 Switching Protocols"
│ ws.onopen()  // 连接建立成功

────────────────────────────────────────────────────────────────────────
点击 Send
│ ws.send(msg) ─────────────────────────►  [接收 WebSocket 数据帧]
│                                         ws_request(c)   // c->status == 1
│                                           ├─ decode_packet()
│                                           │    ├─ 解析 nty_ophdr (FIN, opcode, mask, length)
│                                           │    ├─ 获取 mask_key
│                                           │    └─ demask()  // 解掩码
│                                           ├─ printf()       // 打印解码后的数据
│                                           ├─ encode_packet()
│                                           │    ├─ 填充 nty_ophdr（FIN=1, opcode=1）
│                                           │    └─ 复制数据到发送缓冲
│                                           └─ 设置 c->wlength
│                                         ◄─────── 返回 WebSocket 帧
│ ws.onmessage(event) // 浏览器接收并显示消息

────────────────────────────────────────────────────────────────────────
关闭
│ ws.close()  ──────────────────────────►  服务器关闭连接
│ ws.onclose()
*/