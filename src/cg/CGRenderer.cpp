#include <cg/CGRenderer.h>

#include <iostream>
#include <AntTweakBar.h>
// new
#include <FBXModel.h>
//using mgraphics::fbxmodel::FBXModel;
//using mgraphics::fbxmodel::Mesh;




namespace cgbv
{
	void TW_CALL handleScreenshot(void* data)
	{
		CGRenderer* renderer = reinterpret_cast<CGRenderer*>(data);
		renderer->capture();
	}


	CGRenderer::CGRenderer(GLFWwindow* window) : Renderer(window)
	{

	}


	CGRenderer::~CGRenderer()
	{

	}


	void CGRenderer::capture()
	{
		screenshot.set();
	}


	void CGRenderer::destroy()
	{
		glDeleteVertexArrays(1, &basesurface.vao);
		glDeleteBuffers(1, &basesurface.vbo);

		glDeleteVertexArrays(1, &object.vao);
		glDeleteBuffers(1, &object.vbo);

		glDeleteFramebuffers(1, &framebuffers.shadowmap_buffer);
		glDeleteSamplers(1, &shadowmap_sampler);
	}


	void CGRenderer::resize(int width, int height)
	{
		window_width = width;
		window_height = height;

		glViewport(0, 0, width, height);

		observer_projection = glm::perspective(float(M_PI) / 5.f, float(window_width) / float(window_height), .1f, 200.f);

		TwWindowSize(width, height > 0 ? height : 1);
	}


	void CGRenderer::input(int key, int scancode, int action, int modifiers)
	{
		TwEventCharGLFW(key, action);
		TwEventKeyGLFW(key, action);

		if (action == GLFW_PRESS)
		{
			switch (key)
			{
			case GLFW_KEY_W:
				observer_camera.moveForward(.1f);
				break;
			case GLFW_KEY_S:
				observer_camera.moveForward(-.1f);
				break;
			case GLFW_KEY_A:
				observer_camera.moveRight(.1f);
				break;
			case GLFW_KEY_D:
				observer_camera.moveRight(-.1f);
				break;
			default:
				break;
			}
		}
	}


	bool CGRenderer::setup()
	{
		glfwGetFramebufferSize(window, &window_width, &window_height);


		if (!gladLoadGL())
			return false;


		// GL States
		glClearColor(0.4f, 0.4f, 0.4f, 1.f);

		glEnable(GL_ALPHA_TEST);
		glEnable(GL_DEPTH_TEST);


		// Matrices 
		{
			//observer_projection = glm::perspective(float(M_PI) / 5.f, float(window_width) / float(window_height), .1f, 200.f);
			observer_projection = glm::perspective(glm::pi<float>() / 5.f, float(window_width) / float(window_height), .1f, 20.f);
			observer_camera.setTarget(glm::vec3(0.f, 0.f, 0.f));
			observer_camera.moveTo(0.f, 2.5f, 5.f);

			lightsource_projection = glm::ortho(-8.f, 8.f, -8.f, 8.f, 1.f, 80.f);
			lightsource_camera.setTarget(glm::vec3(0.f, 0.f, 0.f));
			lightsource_camera.moveTo(parameter.lightPos);

			bias = glm::mat4
			(
				0.5, 0.0, 0.0, 0.0,
				0.0, 0.5, 0.0, 0.0,
				0.0, 0.0, 0.5, 0.0,
				0.5, 0.5, 0.5, 1.0
			);
		}




		// Shader
		{
			shader = std::make_unique<cgbv::shader::GLSLShaderprogram>("../shader/VertexShader.glsl", "../shader/FragmentShader.glsl");
			locs.vertex = shader->getAttribLocation("vertex");
			locs.normal = shader->getAttribLocation("normal");
			//locs.uv = shader->getAttribLocation("uvs");
			locs.modelViewProjection = shader->getUniformLocation("matrices.mvp");
			locs.normalmatrix = shader->getUniformLocation("matrices.normal");
			locs.modelview = shader->getUniformLocation("matrices.mv");
			locs.biasedModelViewProjection = shader->getUniformLocation("matrices.bmvp");
			locs.lightPos = shader->getUniformLocation("light.lightPos");
			locs.shadowmap = shader->getUniformLocation("tex.shadowmap");
			locs.lambertFS = shader->getSubroutineIndex(GL_FRAGMENT_SHADER, "lambert");
			locs.depthmapFS = shader->getSubroutineIndex(GL_FRAGMENT_SHADER, "depthmap");
			locs.lightingVS = shader->getSubroutineIndex(GL_VERTEX_SHADER, "verts_and_normals");
			locs.placementVS = shader->getSubroutineIndex(GL_VERTEX_SHADER, "simple_placement");
		}




		// Geometrie
		
		{
			std::vector<glm::vec3> vertices;

			glm::vec3 a(-5.f, 0.f, -5.f);
			glm::vec3 b(5.f, 0.f, -5.f);
			glm::vec3 c(5.f, 0.f, 5.f);
			glm::vec3 d(-5.f, 0.f, 5.f);

			glm::vec3 n(0.f, 1.f, 0.f);

			vertices.push_back(d); vertices.push_back(c); vertices.push_back(a);
			vertices.push_back(a); vertices.push_back(c); vertices.push_back(b);

			std::vector<float> data;
			for (auto v : vertices)
			{
				data.insert(std::end(data), glm::value_ptr(v), glm::value_ptr(v) + sizeof(glm::vec3) / sizeof(float));
				data.insert(std::end(data), glm::value_ptr(n), glm::value_ptr(n) + sizeof(glm::vec3) / sizeof(float));
				basesurface.vertsToDraw++;
			}

			glGenVertexArrays(1, &basesurface.vao);
			glBindVertexArray(basesurface.vao);

			glGenBuffers(1, &basesurface.vbo);
			glBindBuffer(GL_ARRAY_BUFFER, basesurface.vbo);
			glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(locs.vertex);
			glVertexAttribPointer(locs.vertex, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
			glEnableVertexAttribArray(locs.normal);
			glVertexAttribPointer(locs.normal, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)size_t(3 * sizeof(float)));
		}
			/*
			data.clear();
			vertices.clear();
		
			a = glm::vec3(-1.f, 0.f, 0.f);
			b = glm::vec3(1.f, 0.f, 0.f);
			c = glm::vec3(0.f, 2.f, 0.f);

			n = glm::vec3(0.f, 0.f, 1.f);

			vertices.push_back(a); vertices.push_back(b); vertices.push_back(c);

			for (auto v : vertices)
			{
				data.insert(std::end(data), glm::value_ptr(v), glm::value_ptr(v) + sizeof(glm::vec3) / sizeof(float));
				data.insert(std::end(data), glm::value_ptr(n), glm::value_ptr(n) + sizeof(glm::vec3) / sizeof(float));
				object.vertsToDraw++;
			}

			glGenVertexArrays(1, &object.vao);
			glBindVertexArray(object.vao);

			glGenBuffers(1, &object.vbo);
			glBindBuffer(GL_ARRAY_BUFFER, object.vbo);
			glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);

			glEnableVertexAttribArray(locs.vertex);
			glVertexAttribPointer(locs.vertex, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
			glEnableVertexAttribArray(locs.normal);
			glVertexAttribPointer(locs.normal, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (const void*)size_t(3 * sizeof(float)));
		}*/

		// FBX Importer
		{
			std::vector<std::string> fbxs;

			std::string bunny = "../fbxmodels/stanford-bunny_maya_export.fbx";
			fbxs.push_back(bunny);
			std::string budda = "../fbxmodels/happy-buddha_maya_export.fbx";
			fbxs.push_back(budda);
			std::string dragon = "";
			fbxs.push_back(dragon);
			std::string cuboid = "../fbxmodels/Box.fbx";
			fbxs.push_back(cuboid);
			std::string cone = "";
			fbxs.push_back(cone);
			std::string cylinder = "";
			fbxs.push_back(cylinder);

			auto fbx = cgbv::fbxmodel::FBXModel::FBXModel(bunny);
			////FBXmodel fbx(bunny);
			////GLuint triVAO, triVBO;

			////glGenVertexArrays(1, &triVAO);
			////glGenBuffers(1, &triVBO);

			////glBindVertexArray(triVAO);

			for (auto mesh : fbx.Meshes())
			{
				/*
					glBindBuffer(GL_ARRAY_BUFFER, triVBO);
					glBufferData(GL_ARRAY_BUFFER, (3 * mesh.VertexCount() + 3 * mesh.NormalCount() + 2 * mesh.UVDataCount() + 3 * mesh.TangentCount() + 3 * mesh.BitangentCount()) * sizeof(float), nullptr, GL_STATIC_DRAW);
					/*
					const float* data = mesh.VertexData();
					glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * mesh.VertexCount() * sizeof(float), data);
					data = mesh.NormalData();
					glBufferSubData(GL_ARRAY_BUFFER, (mesh.VertexCount() * 3) * sizeof(float), 3 * mesh.NormalCount() * sizeof(float), data);
					data = mesh.UVData();
					glBufferSubData(GL_ARRAY_BUFFER, (mesh.VertexCount() * 3 + mesh.NormalCount() * 3) * sizeof(float), 2 * mesh.UVDataCount() * sizeof(float), data);
					data = mesh.TangentData();
					glBufferSubData(GL_ARRAY_BUFFER, (mesh.VertexCount() * 3 + mesh.NormalCount() * 3 + mesh.UVDataCount() * 2) * sizeof(float), 3 * mesh.TangentCount() * sizeof(float), data);
					data = mesh.BitangentData();
					glBufferSubData(GL_ARRAY_BUFFER, (mesh.VertexCount() * 3 + mesh.NormalCount() * 3 + mesh.UVDataCount() * 2 + mesh.TangentCount() * 3) * sizeof(float), 3 * mesh.BitangentCount() * sizeof(float), data);

					glEnableVertexAttribArray(locs.vertex);// 0);
					glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

					glEnableVertexAttribArray(locs.normal); // 1);
					glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (const void*)((3 * mesh.VertexCount()) * sizeof(float)));

					/*glEnableVertexAttribArray(2);
					glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void*)((3 * mesh.VertexCount() + 3 * mesh.NormalCount()) * sizeof(float)));

					glEnableVertexAttribArray(3);
					glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (const void*)((3 * mesh.VertexCount() + 3 * mesh.NormalCount() + 2 * mesh.UVDataCount()) * sizeof(float)));

					glEnableVertexAttribArray(4);
					glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, (const void*)((3 * mesh.VertexCount() + 3 * mesh.NormalCount() + 2 * mesh.UVDataCount() + 3 * mesh.TangentCount()) * sizeof(float)));
				*/

				glGenVertexArrays(1, &object.vao);
				glBindVertexArray(object.vao);

				glGenBuffers(1, &object.vbo);
				glBindBuffer(GL_ARRAY_BUFFER, object.vbo);
				////glBufferData(GL_ARRAY_BUFFER, mesh.VertexCount()*3 * sizeof(float), data,  GL_STATIC_DRAW);

				const float* data = mesh.VertexData();
				glBufferSubData(GL_ARRAY_BUFFER, 0, 3 * mesh.VertexCount() * sizeof(float), data);
				data = mesh.NormalData();
				glBufferSubData(GL_ARRAY_BUFFER, (mesh.VertexCount() * 3) * sizeof(float), 3 * mesh.NormalCount() * sizeof(float), data);

				glEnableVertexAttribArray(locs.vertex);
				glVertexAttribPointer(locs.vertex, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

				glEnableVertexAttribArray(locs.normal);
				glVertexAttribPointer(locs.normal, 3, GL_FLOAT, GL_FALSE, mesh.VertexCount() * 3 * sizeof(float), nullptr);

				object.vertsToDraw = mesh.VertexCount();
			}
	}
		

		// GUI
		{
			TwInit(TW_OPENGL_CORE, nullptr);
			TwWindowSize(1280, 720);
			TwBar* tweakbar = TwNewBar("TweakBar");
			TwDefine(" TweakBar size='300 400'");

			TwAddVarRW(tweakbar, "Global Rotation", TW_TYPE_QUAT4F, &parameter.globalRotation, "showval=false opened=true");
			TwAddButton(tweakbar, "Take Screenshot", handleScreenshot, this, nullptr);
			TwAddVarRW(tweakbar, "Lichtrichtung", TW_TYPE_DIR3F, &parameter.lightPos, "group=Light axisx=-x axisy=-y axisz=-z opened=true");
		}




		// Framebuffer
		{
			shadowmap = std::make_unique<cgbv::textures::Texture>();

			glGenFramebuffers(1, &framebuffers.shadowmap_buffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.shadowmap_buffer);
			
			glBindTexture(GL_TEXTURE_2D, shadowmap->getTextureID());
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, shadowmap_width, shadowmap_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

			glGenSamplers(1, &shadowmap_sampler);
			glSamplerParameteri(shadowmap_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(shadowmap_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(shadowmap_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(shadowmap_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(shadowmap_sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);

			glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowmap->getTextureID(), 0);
			glDrawBuffer(GL_NONE);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cout << "Shadowmap Framebuffer failed" << std::endl;
		}



		return true;
	}


	void CGRenderer::render()
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1.4f, 1.f);
		shadowmap_pass();
		final_pass();
	}


	void CGRenderer::shadowmap_pass()
	{
		glViewport(0, 0, shadowmap_width, shadowmap_height);

		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.shadowmap_buffer);

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

		model = glm::mat4_cast(parameter.globalRotation);

		glm::mat4 shadow_view = lightsource_camera.getViewMatrix();

		shader->use();
		glUniformMatrix4fv(locs.modelViewProjection, 1, GL_FALSE, glm::value_ptr(lightsource_projection * shadow_view * model));

		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &locs.placementVS);
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &locs.depthmapFS);

		glBindVertexArray(basesurface.vao);
		glDrawArrays(GL_TRIANGLES, 0, basesurface.vertsToDraw);

		glBindVertexArray(object.vao);
		glDrawArrays(GL_TRIANGLES, 0, object.vertsToDraw);
	}


	void CGRenderer::final_pass()
	{
		glViewport(0, 0, window_width, window_height);

		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers.default_buffer);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		glm::mat4 view = observer_camera.getViewMatrix();
		//glm::mat4 view = lightsource_camera.getViewMatrix();

		model = glm::mat4_cast(parameter.globalRotation);

		shader->use();
		normal = glm::transpose(glm::inverse(view * model));
		glUniformMatrix4fv(locs.modelViewProjection, 1, GL_FALSE, glm::value_ptr(observer_projection * view * model));
		//glUniformMatrix4fv(locs.modelViewProjection, 1, GL_FALSE, glm::value_ptr(lightsource_projection * view * model));
		glUniformMatrix4fv(locs.modelview, 1, GL_FALSE, glm::value_ptr(view * model));
		glUniformMatrix3fv(locs.normalmatrix, 1, GL_FALSE, glm::value_ptr(normal));

		glm::mat4 shadow_view = lightsource_camera.getViewMatrix();
		glUniformMatrix4fv(locs.biasedModelViewProjection, 1, GL_FALSE, glm::value_ptr(bias * lightsource_projection * shadow_view * model));

		glUniform3fv(locs.lightPos, 1, glm::value_ptr(parameter.lightPos));

		glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &locs.lightingVS);
		glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &locs.lambertFS);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, shadowmap->getTextureID());
		glBindSampler(0, shadowmap_sampler);
		glUniform1i(locs.shadowmap, 0);

		glBindVertexArray(basesurface.vao);
		glDrawArrays(GL_TRIANGLES, 0, basesurface.vertsToDraw);

		glBindVertexArray(object.vao);
		glDrawArrays(GL_TRIANGLES, 0, object.vertsToDraw);

		if (screenshot[0])
		{
			std::cout << "Storing Shadowmap to Disk...";
			std::unique_ptr<GLubyte[]> pixel = std::make_unique<GLubyte[]>(shadowmap_width * shadowmap_height);
			glGetTextureImage(shadowmap->getTextureID(), 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, shadowmap_width * shadowmap_height, pixel.get());

			cgbv::textures::Texture2DStorage::StoreGreyscale("../shadowmap.png", pixel.get(), shadowmap_width, shadowmap_height, 0);
			std::cout << "done" << std::endl;
			screenshot.reset();
		}

		TwDraw();
	}


	void CGRenderer::update()
	{
		lightsource_camera.moveTo(parameter.lightPos);
	}
}