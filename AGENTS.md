To use this application (deepreinforce-ai/Ornith-1.0-9B: Chat with an AI coding assistant that explains its reasoning):
API schema: GET https://deepreinforce-ai-ornith-1-0-9b.hf.space/gradio_api/info
Call endpoint: POST https://deepreinforce-ai-ornith-1-0-9b.hf.space/gradio_api/call/v2/{endpoint} {"param_name": value, ...}
Poll result: GET https://deepreinforce-ai-ornith-1-0-9b.hf.space/gradio_api/call/{endpoint}/{event_id}
File inputs: POST https://deepreinforce-ai-ornith-1-0-9b.hf.space/gradio_api/upload -F "files=@file.ext", use as: {"path": "<returned-path>", "meta": {"_type": "gradio.FileData"}, "orig_name": "file.ext"}
Auth: Bearer $HF_TOKEN (https://huggingface.co/settings/tokens)