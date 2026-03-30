#ifndef I_RENDER_PIPELINE_H
#define I_RENDER_PIPELINE_H

#include "RenderObject.h"
#include "RenderView.h"

class IRenderPipeline
{
public:
	virtual ~IRenderPipeline() = default;
	virtual void Render(nvrhi::CommandListHandle cmd, nvrhi::FramebufferHandle framebuffer, const& RenderView view) = 0;
};

#endif // I_RENDER_PIPELINE_H