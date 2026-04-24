export interface Env {
    ROOMS: KVNamespace;
}

// Characters that are visually unambiguous (no 0/O, 1/I/L)
const CHARS = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const CODE_LEN = 6;
const TTL = 300; // 5 minutes — long enough to finish the handshake

function generateCode(): string {
    let code = '';
    for (let i = 0; i < CODE_LEN; i++)
        code += CHARS[Math.floor(Math.random() * CHARS.length)];
    return code;
}

function ok(body: string, status = 200): Response {
    return new Response(body, {
        status,
        headers: { 'Access-Control-Allow-Origin': '*' },
    });
}

function err(msg: string, status: number): Response {
    return new Response(msg, {
        status,
        headers: { 'Access-Control-Allow-Origin': '*' },
    });
}

export default {
    async fetch(request: Request, env: Env): Promise<Response> {
        // CORS preflight
        if (request.method === 'OPTIONS') {
            return new Response(null, {
                headers: {
                    'Access-Control-Allow-Origin': '*',
                    'Access-Control-Allow-Methods': 'GET, POST',
                },
            });
        }

        const parts = new URL(request.url).pathname.split('/').filter(Boolean);
        const [route, code] = parts;

        // ── POST /host  ──────────────────────────────────────────────────────
        // Body: offer SDP.  Returns: 6-char room code.
        if (request.method === 'POST' && route === 'host') {
            const offer = await request.text();
            if (!offer) return err('Missing offer', 400);

            for (let attempt = 0; attempt < 10; attempt++) {
                const candidate = generateCode();
                if (!(await env.ROOMS.get(`offer:${candidate}`))) {
                    await env.ROOMS.put(`offer:${candidate}`, offer, { expirationTtl: TTL });
                    return ok(candidate);
                }
            }
            return err('Could not generate unique room', 500);
        }

        // ── GET /join/{code}  ────────────────────────────────────────────────
        // Returns: offer SDP, or 404 if room not found.
        if (request.method === 'GET' && route === 'join' && code) {
            const offer = await env.ROOMS.get(`offer:${code.toUpperCase()}`);
            if (!offer) return err('Room not found', 404);
            return ok(offer);
        }

        // ── POST /answer/{code}  ─────────────────────────────────────────────
        // Body: answer SDP.  Stores it so the host can poll for it.
        if (request.method === 'POST' && route === 'answer' && code) {
            const key = code.toUpperCase();
            if (!(await env.ROOMS.get(`offer:${key}`))) return err('Room not found', 404);
            const answer = await request.text();
            if (!answer) return err('Missing answer', 400);
            await env.ROOMS.put(`answer:${key}`, answer, { expirationTtl: TTL });
            return ok('OK');
        }

        // ── GET /answer/{code}  ──────────────────────────────────────────────
        // Returns: answer SDP once available, or 204 if not ready yet.
        if (request.method === 'GET' && route === 'answer' && code) {
            const answer = await env.ROOMS.get(`answer:${code.toUpperCase()}`);
            if (!answer) return new Response('', { status: 204, headers: { 'Access-Control-Allow-Origin': '*' } });
            return ok(answer);
        }

        return err('Not found', 404);
    },
};
