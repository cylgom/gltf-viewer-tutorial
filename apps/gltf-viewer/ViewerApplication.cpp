#include "ViewerApplication.hpp"

#include <iostream>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "utils/cameras.hpp"
#include "utils/gltf.hpp"
#include "utils/images.hpp"

#include <stb_image.h>
#include <stb_image_write.h>
#include <tiny_gltf.h>

#define VERTEX_ATTRIB_POSITION_IDX 0
#define VERTEX_ATTRIB_NORMAL_IDX 1
#define VERTEX_ATTRIB_TEXCOORD0_IDX 2

void keyCallback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, 1);
  }
}

bool ViewerApplication::loadGltfFile(tinygltf::Model& model)
{
	std::string err;
	std::string warn;

	bool ret = m_gltfLoader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());

	if (!err.empty())
	{
		std::cerr << "Error: " << err << std::endl;
	}

	if (!warn.empty())
	{
		std::cerr << "Warning: " << warn << std::endl;
	}

	return ret;
}

GLuint ViewerApplication::loadEnvTexture()
{
	GLuint envTexture = 0;

	if (!m_cubeMapFilePath.string().empty())
	{
		stbi_set_flip_vertically_on_load(true);

		int components;
		int width;
		int height;

		float *data = stbi_loadf(
			m_cubeMapFilePath.c_str(),
			&width,
			&height,
			&components,
			0);

		if (data)
		{
			glGenTextures(1, &envTexture);
			glBindTexture(GL_TEXTURE_2D, envTexture);

			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_RGB16F,
				width,
				height,
				0,
				GL_RGB,
				GL_FLOAT,
				data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			stbi_image_free(data);
		}
		else
		{
			std::cerr << "Failed to load cubemap" << std::endl;
		}
	}

	return envTexture;
}

GLuint ViewerApplication::loadCorrectedEnvTexture()
{
	// prepare framebuffer to render to texture
	GLuint captureFBO;
	GLuint captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	// prepare output texture
	GLuint envTexture;
	glGenTextures(1, &envTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, envTexture);

	for (GLuint i = 0; i < 6; ++i)
	{
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0,
			GL_RGB16F,
			512,
			512,
			0,
			GL_RGB,
			GL_FLOAT,
			nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// prepare camera positions for texture rendering
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	glm::mat4 captureViews[] = 
	{
		glm::lookAt(
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3( 1.0f,  0.0f,  0.0f),
			glm::vec3(0.0f, -1.0f,  0.0f)),

		glm::lookAt(
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(-1.0f,  0.0f,  0.0f),
			glm::vec3(0.0f, -1.0f,  0.0f)),

		glm::lookAt(
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3( 0.0f,  1.0f,  0.0f),
			glm::vec3(0.0f,  0.0f,  1.0f)),

		glm::lookAt(
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3( 0.0f, -1.0f,  0.0f),
			glm::vec3(0.0f,  0.0f, -1.0f)),

		glm::lookAt(
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3( 0.0f,  0.0f,  1.0f),
			glm::vec3(0.0f, -1.0f,  0.0f)),

		glm::lookAt(
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3( 0.0f,  0.0f, -1.0f),
			glm::vec3(0.0f, -1.0f,  0.0f))
	};

	// cubemap-correction shaders
	const auto glslCubemapProgram =
		compileProgram({
			m_ShadersRootPath / m_AppName / m_cubemapVertexShader,
			m_ShadersRootPath / m_AppName / m_cubemapFragmentShader});

	const auto cubemapEquirectangularMapLocation =
		glGetUniformLocation(glslCubemapProgram.glId(), "uEquirectangularMap");
	const auto cubemapModelProjMatrixLocation =
		glGetUniformLocation(glslCubemapProgram.glId(), "uModelProjMatrix");
	const auto cubemapModelViewMatrixLocation =
		glGetUniformLocation(glslCubemapProgram.glId(), "uModelViewMatrix");

	glslCubemapProgram.use();
	glUniform1i(cubemapEquirectangularMapLocation, 0);
	glUniformMatrix4fv(
		cubemapModelProjMatrixLocation,
		1,
		GL_FALSE,
		glm::value_ptr(captureProjection));

	// load non-corrected cubemap texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, loadEnvTexture());

	// render corrected cubemap texture
	glViewport(0, 0, 512, 512);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	for (unsigned int i = 0; i < 6; ++i)
	{
		glUniformMatrix4fv(
			cubemapModelViewMatrixLocation,
			1,
			GL_FALSE,
			glm::value_ptr(captureViews[i]));

		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0, 
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			envTexture,
			0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube();
	}

	// restore framebuffer state
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return envTexture;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(const tinygltf::Model& model)
{
	size_t len = model.buffers.size();

	std::vector<GLuint> bo(len, 0);
	glGenBuffers(len, bo.data());

	for (size_t i = 0; i < len; ++i)
	{
		glBindBuffer(GL_ARRAY_BUFFER, bo[i]);

		glBufferStorage(
			GL_ARRAY_BUFFER,
			model.buffers[i].data.size(),
			model.buffers[i].data.data(),
			0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return bo;
}

void ViewerApplication::initCube()
{
	float vertices[] =
	{
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		// bottom face
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		// top face
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};

	glGenVertexArrays(1, &m_unitCubeVAO);
	glGenBuffers(1, &m_unitCubeVBO);

	glBindBuffer(
		GL_ARRAY_BUFFER,
		m_unitCubeVBO);
	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof (vertices),
		vertices,
		GL_STATIC_DRAW);

	glBindVertexArray(m_unitCubeVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0,
		3,
		GL_FLOAT,
		GL_FALSE,
		8 * sizeof (float),
		(void*) 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		1,
		3,
		GL_FLOAT,
		GL_FALSE,
		8 * sizeof (float),
		(void*) (3 * sizeof (float)));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(
		2,
		2,
		GL_FLOAT,
		GL_FALSE,
		8 * sizeof (float),
		(void*) (6 * sizeof (float)));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void ViewerApplication::renderCube()
{
    glBindVertexArray(m_unitCubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void vao_init(
	const tinygltf::Model& model,
	const tinygltf::Primitive& primitive,
	const std::vector<GLuint>& bufferObjects,
	const char* str,
	const GLuint index)
{
	const auto iterator = primitive.attributes.find(str);

	// If "POSITION" has been found in the map
	if (iterator != end(primitive.attributes))
	{
		// (*iterator).first is the key "POSITION", (*iterator).second
		// is the value, ie. the index of the accessor for this attribute
		const auto accessorIdx = (*iterator).second;

		// TODO get the correct tinygltf::Accessor from model.accessors
		const auto& accessor = model.accessors[accessorIdx];

		// TODO get the correct tinygltf::BufferView from model.bufferViews. You need use the accessor
		const auto& bufferView = model.bufferViews[accessor.bufferView];

		// TODO get the index of the buffer used by the bufferView (you need to use it)
		const auto bufferIdx = bufferView.buffer;

		// TODO get the correct buffer object from the buffer index
		const auto bufferObject = model.buffers[bufferIdx];

		// TODO Enable the vertex attrib array corresponding to POSITION with glEnableVertexAttribArray
		// (you need to use VERTEX_ATTRIB_POSITION_IDX which is defined at the top of the file)
		glEnableVertexAttribArray(index);

		// TODO Bind the buffer object to GL_ARRAY_BUFFER
		glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);

		// TODO Compute the total byte offset using the accessor and the buffer view
		const auto byteOffset = bufferView.byteOffset + accessor.byteOffset;

		// TODO Call glVertexAttribPointer with the correct arguments. 
		// Remember size is obtained with accessor.type, type is obtained with accessor.componentType. 
		// The stride is obtained in the bufferView, normalized is always GL_FALSE, and pointer is the byteOffset (cast).
		glVertexAttribPointer(
			index,
			accessor.type,
			accessor.componentType,
			GL_FALSE,
			bufferView.byteStride,
			(const GLvoid*) byteOffset);
	}
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects(
	const tinygltf::Model& model,
	const std::vector<GLuint>& bufferObjects,
	std::vector<VaoRange>& meshIndexToVaoRange)
{
	std::vector<GLuint> vertexArrayObjects(0, 0);

	size_t offset;
	size_t primitive_len;
	size_t mesh_len = model.meshes.size();

	meshIndexToVaoRange.resize(mesh_len);

	for (size_t i = 0; i < mesh_len; ++i)
	{
		offset = vertexArrayObjects.size();
		primitive_len = model.meshes[i].primitives.size();

		meshIndexToVaoRange[i].begin = (GLsizei) offset,
		meshIndexToVaoRange[i].count = (GLsizei) primitive_len,

		vertexArrayObjects.resize(offset + primitive_len);
		glGenVertexArrays(primitive_len, vertexArrayObjects.data() + offset);

		for (size_t k = 0; k < primitive_len; ++k)
		{
			glBindVertexArray(vertexArrayObjects[offset + k]);
			const tinygltf::Primitive& primitive = model.meshes[i].primitives[k];
			vao_init(model, primitive, bufferObjects, "POSITION", VERTEX_ATTRIB_POSITION_IDX);
			vao_init(model, primitive, bufferObjects, "NORMAL", VERTEX_ATTRIB_NORMAL_IDX);
			vao_init(model, primitive, bufferObjects, "TEXCOORD_0", VERTEX_ATTRIB_TEXCOORD0_IDX);

			if (primitive.indices >= 0)
			{
				const auto& accessor = model.accessors[primitive.indices];
				const auto& bufferView = model.bufferViews[accessor.bufferView];

				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[bufferView.buffer]);
			}
		}
	}

	glBindVertexArray(0);

	return vertexArrayObjects;
}

std::vector<GLuint> ViewerApplication::createTextureObjects(const tinygltf::Model &model) const
{
	size_t count = model.textures.size();
	std::vector<GLuint> textureObjects(count);

	for (size_t i = 0; i < count; ++i)
	{
		glGenTextures(1, &(textureObjects[i]));
		glBindTexture(GL_TEXTURE_2D, textureObjects[i]);

		// get texture
		const auto& texture = model.textures[i];

		// make image
		assert(texture.source >= 0);
		const auto& image = model.images[texture.source];

		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			image.width,
			image.height,
			0,
			GL_RGBA,
			image.pixel_type,
			image.image.data());

		// sampler
		tinygltf::Sampler defaultSampler;
		defaultSampler.minFilter = GL_LINEAR;
		defaultSampler.magFilter = GL_LINEAR;
		defaultSampler.wrapS = GL_REPEAT;
		defaultSampler.wrapT = GL_REPEAT;
		defaultSampler.wrapR = GL_REPEAT;

		const auto &sampler =
			(texture.sampler >= 0) ?
				model.samplers[texture.sampler] : defaultSampler;

		// parameters
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER,
			(sampler.minFilter != -1) ?
				sampler.minFilter : GL_LINEAR);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER,
			(sampler.magFilter != -1) ?
				sampler.magFilter : GL_LINEAR);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_WRAP_S,
			sampler.wrapS);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_WRAP_T,
			sampler.wrapT);
		glTexParameteri(
			GL_TEXTURE_2D,
			GL_TEXTURE_WRAP_R,
			sampler.wrapR);

		if((sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST)
		|| (sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR)
		|| (sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST)
		|| (sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR))
		{
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	return textureObjects;
}

int ViewerApplication::run()
{
  // Loader shaders
  const auto glslProgram =
      compileProgram({
		  m_ShadersRootPath / m_AppName / m_vertexShader,
          m_ShadersRootPath / m_AppName / m_fragmentShader});

  const auto modelViewProjMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
  const auto modelViewMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
  const auto normalMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");
  const auto lightDirectionLocation =
      glGetUniformLocation(glslProgram.glId(), "uLightDirection");
  const auto lightRadianceLocation =
      glGetUniformLocation(glslProgram.glId(), "uLightIntensity");
  const auto baseColorLocation =
      glGetUniformLocation(glslProgram.glId(), "uBaseColorTexture");
  const auto baseColorFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uBaseColorFactor");

  const auto metallicFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uMetallicFactor");
  const auto roughnessFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uRoughnessFactor");
  const auto metallicRoughnessTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uMetallicRoughnessTexture");

  const auto emissiveTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uEmissiveTexture");
  const auto emissiveFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uEmissiveFactor");

  const auto occlusionTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionTexture");
  const auto occlusionStrengthLocation =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionStrength");

  const auto normalTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalTexture");
  const auto normalScaleLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalScale");

  // Skybox
  const auto glslSkyboxProgram =
      compileProgram({
		  m_ShadersRootPath / m_AppName / m_skyboxVertexShader,
          m_ShadersRootPath / m_AppName / m_skyboxFragmentShader});

  const auto skyboxEquirectangularMapLocation =
      glGetUniformLocation(glslSkyboxProgram.glId(), "uEquirectangularMap");
  const auto skyboxModelProjMatrixLocation =
      glGetUniformLocation(glslSkyboxProgram.glId(), "uModelProjMatrix");
  const auto skyboxModelViewMatrixLocation =
      glGetUniformLocation(glslSkyboxProgram.glId(), "uModelViewMatrix");

  // TODO Loading the glTF file
  tinygltf::Model model;

  if (!loadGltfFile(model)) {
	  std::cerr << "Failed to load glTF model" << std::endl;

	  return -1;
  }

  // TODO Implement a new CameraController model and use it instead.
  glm::vec3 bboxMin;
  glm::vec3 bboxMax;

  computeSceneBounds(model, bboxMin, bboxMax);

  glm::vec3 diag = bboxMax - bboxMin;

  // Build projection matrix
  auto maxDistance = glm::length(diag);
  maxDistance = maxDistance > 0.f ? maxDistance : 100.f;

  const auto projMatrix = glm::perspective(
		  70.f,
		  float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance,
		  1.5f * maxDistance);

  int controlsType = 0;
  bool lightHeader = false;
  bool lightFromCamera = true;
  float lightAngleH = 1.55f;
  float lightAngleV = 2.0f;

  glm::vec3 lightDirectionRaw = glm::vec3(1.0f, 1.0f, 1.0f);
  glm::vec3 lightRadiance = glm::vec3(1.0f, 1.0f, 1.0f);

  std::unique_ptr<CameraController> cameraController(
	  std::make_unique<TrackballCameraController>(
		  m_GLFWHandle.window()));

  if (m_hasUserCamera)
  {
    cameraController->setCamera(m_userCamera);
  }
  else
  {
    // TODO Use scene bounds to compute a better default camera
	glm::vec3 eye;
	glm::vec3 up(0, 1, 0);
	glm::vec3 center = bboxMin + (0.5f * diag);

	if (diag.z > 0)
	{
		eye = center + diag;
	}
	else
	{
		eye = center + 2.f * glm::cross(diag, up);
	}

    cameraController->setCamera(Camera{eye, center, up});
  }

  // Physically-Based Materials
  const std::vector<GLuint> textureObjects = createTextureObjects(model);
  GLuint whiteTexture;

  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);

  float white[] = {1, 1, 1, 1};

  glTexImage2D(
	  GL_TEXTURE_2D,
	  0,
	  GL_RGBA,
	  1,
	  1,
	  0,
	  GL_RGBA,
	  GL_FLOAT,
	  white);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);

  // Cubemap
  initCube();
  GLuint envTexture = loadCorrectedEnvTexture();

  // Reset
  glBindTexture(GL_TEXTURE_2D, 0);

  // Creation of Buffer Objects
  const std::vector<GLuint> bufferObjects = createBufferObjects(model);

  // Creation of Vertex Array Objects
  std::vector<VaoRange> meshIndexToVaoRange;

  const std::vector<GLuint> vertexArrayObjects = createVertexArrayObjects(
		model,
		bufferObjects,
		meshIndexToVaoRange);

  // Setup OpenGL state for rendering
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

	const auto bindMaterial = [&](const auto materialIndex)
	{
		if (materialIndex >= 0)
		{
			const auto &material =
				model.materials[materialIndex];

			const auto &pbrMetallicRoughness =
				material.pbrMetallicRoughness;

			const auto &emissiveTexture =
				material.emissiveTexture;

			const auto &emissiveFactor =
				material.emissiveFactor;

			const auto &occlusionTexture =
				material.occlusionTexture;

			const auto &normalTexture =
				material.normalTexture;

			if (model.textures.size() > 0)
			{
				const auto &texture = model.textures[
					pbrMetallicRoughness.baseColorTexture.index];

				// base color
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(
					GL_TEXTURE_2D,
					textureObjects[texture.source]);
				glUniform1i(baseColorLocation, 0);

				// metallic roughness
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(
					GL_TEXTURE_2D,
					textureObjects[
						pbrMetallicRoughness.metallicRoughnessTexture.index]);
				glUniform1i(metallicRoughnessTextureLocation, 1);
				
				glUniform4f(
					baseColorFactorLocation,
					pbrMetallicRoughness.baseColorFactor[0],
					pbrMetallicRoughness.baseColorFactor[1],
					pbrMetallicRoughness.baseColorFactor[2],
					pbrMetallicRoughness.baseColorFactor[3]);

				glUniform1f(
					metallicFactorLocation,
					pbrMetallicRoughness.metallicFactor);

				glUniform1f(
					roughnessFactorLocation,
					pbrMetallicRoughness.roughnessFactor);

				// emissive
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(
					GL_TEXTURE_2D,
					textureObjects[emissiveTexture.index]);
				glUniform1i(emissiveTextureLocation, 2);

				glUniform3f(
					emissiveFactorLocation,
					emissiveFactor[0],
					emissiveFactor[1],
					emissiveFactor[2]);

				// occlusion
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(
					GL_TEXTURE_2D,
					textureObjects[occlusionTexture.index]);
				glUniform1i(occlusionTextureLocation, 3);

				glUniform1f(
					occlusionStrengthLocation,
					occlusionTexture.strength);

				// normal map
				glActiveTexture(GL_TEXTURE4);
				glBindTexture(
					GL_TEXTURE_2D,
					textureObjects[normalTexture.index]);
				glUniform1i(normalTextureLocation, 4);

				glUniform1f(
					normalScaleLocation,
					normalTexture.scale);

				return;
			}
		}

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, whiteTexture);
		glUniform1i(baseColorLocation, 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUniform1i(metallicRoughnessTextureLocation, 1);
		glUniform4f(baseColorFactorLocation, 1, 1, 1, 1);
		glUniform1f(metallicFactorLocation, 0);
		glUniform1f(roughnessFactorLocation, 0);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUniform3f(emissiveFactorLocation, 0, 0, 0);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUniform1f(occlusionStrengthLocation, 1);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUniform1f(normalScaleLocation, 1);
	};

	// Lambda function to draw the scene
	const auto drawScene = [&](const Camera &camera)
	{
		glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const auto viewMatrix = camera.getViewMatrix();

		// Environment skybox
		const auto drawSkybox = [&]()
		{
			glActiveTexture(GL_TEXTURE0);

			glBindTexture(
				GL_TEXTURE_CUBE_MAP,
				envTexture);

			glUniform1i(skyboxEquirectangularMapLocation, 0);

			glUniformMatrix4fv(
				skyboxModelProjMatrixLocation,
				1,
				GL_FALSE,
				glm::value_ptr(projMatrix));

			glUniformMatrix4fv(
				skyboxModelViewMatrixLocation,
				1,
				GL_FALSE,
				glm::value_ptr(viewMatrix));

			renderCube();
		};

		// The recursive function that should draw a node
		// We use a std::function because a simple lambda cannot be recursive
		const std::function<void(int, const glm::mat4 &)> drawNode =
		[&](int nodeIdx, const glm::mat4 &parentMatrix)
		{
			// TODO The drawNode function
			tinygltf::Node& node = model.nodes[nodeIdx];
			glm::mat4 modelMatrix = getLocalToWorldMatrix(
				node,
				parentMatrix);

			if (node.mesh >= 0)
			{
				glm::mat4 modelViewMatrix = viewMatrix * modelMatrix;
				glm::mat4 modelViewProjectionMatrix = projMatrix * modelViewMatrix;
				glm::mat4 normalMatrix = glm::inverse(glm::transpose(modelViewMatrix));

				glm::vec3 lightDirection;

				if (lightFromCamera)
				{
					lightDirection = glm::vec3(0.0f, 0.0f, 1.0f);
				}
				else
				{
					lightDirection = glm::normalize(viewMatrix * glm::vec4(lightDirectionRaw, 0.0f));
				}

				glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewMatrix));
				glUniformMatrix4fv(modelViewProjMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewProjectionMatrix));
				glUniformMatrix4fv(normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));

				if (lightDirectionLocation >= 0)
				{
					glUniform3f(lightDirectionLocation,
						lightDirection[0],
						lightDirection[1],
						lightDirection[2]);
				}

				if (lightRadianceLocation >= 0)
				{
					glUniform3f(lightRadianceLocation,
						lightRadiance[0],
						lightRadiance[1],
						lightRadiance[2]);
				}
			}

			tinygltf::Mesh& mesh = model.meshes[node.mesh];
			struct VaoRange& range = meshIndexToVaoRange[node.mesh];

			for (size_t i = 0; i < range.count; ++i)
			{
				bindMaterial(mesh.primitives[i].material);
				glBindVertexArray(vertexArrayObjects[range.begin + i]);
				tinygltf::Primitive& primitive = mesh.primitives[i];

				if (primitive.indices >= 0)
				{
					const auto& accessor = model.accessors[primitive.indices];
					const auto& bufferView = model.bufferViews[accessor.bufferView];
					const auto byteOffset = bufferView.byteOffset + accessor.byteOffset;

					glDrawElements(primitive.mode, accessor.count, accessor.componentType, (const GLvoid*) byteOffset);
				}
				else
				{
					const auto accessorIdx = (*begin(primitive.attributes)).second;
					const auto &accessor = model.accessors[accessorIdx];

					glDrawArrays(primitive.mode, 0, accessor.count);
				}
			}
		};

		// Draw the scene referenced by gltf file
		if (model.defaultScene >= 0)
		{
			// Draw skybox
			glslSkyboxProgram.use();
			drawSkybox();

			// Draw all nodes
			glslProgram.use();

			for (size_t i = 0; i < model.scenes[model.defaultScene].nodes.size(); ++i)
			{
				drawNode(model.scenes[model.defaultScene].nodes[i], glm::mat4(1));
			}
		}

		glBindVertexArray(0);
	};

	if (!m_OutputPath.empty())
	{
		const auto strPath = m_OutputPath.string();
		std::vector<unsigned char> pixels(m_nWindowWidth * m_nWindowHeight * 3, 0);

		renderToImage(
			m_nWindowWidth,
			m_nWindowHeight,
			3,
			pixels.data(),
			[&]()
			{
				drawScene(cameraController->getCamera());
			});

		flipImageYAxis(
			m_nWindowWidth,
			m_nWindowHeight,
			3,
			pixels.data());

		stbi_write_png(
			strPath.c_str(),
			m_nWindowWidth,
			m_nWindowHeight,
			3,
			pixels.data(),
			0);

		return 0;
	}

  // Loop until the user closes the window
  for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
       ++iterationCount) {
    const auto seconds = glfwGetTime();

    const auto camera = cameraController->getCamera();
    drawScene(camera);

    // GUI code:
    imguiNewFrame();

    {
      ImGui::Begin("GUI");
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
          1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y,
            camera.eye().z);
        ImGui::Text("center: %.3f %.3f %.3f", camera.center().x,
            camera.center().y, camera.center().z);
        ImGui::Text(
            "up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);

        ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y,
            camera.front().z);
        ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y,
            camera.left().z);

        if (ImGui::Button("CLI camera args to clipboard")) {
          std::stringstream ss;
          ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
             << camera.eye().z << "," << camera.center().x << ","
             << camera.center().y << "," << camera.center().z << ","
             << camera.up().x << "," << camera.up().y << "," << camera.up().z;
          const auto str = ss.str();
          glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
        }

		ImGui::Text("Controls type");

        if (ImGui::RadioButton("Trackball", &controlsType, 0)
		|| ImGui::RadioButton("First-person", &controlsType, 1))
		{
			const Camera& oldCamera = cameraController->getCamera();

			if (controlsType == 0)
			{
				cameraController = std::make_unique<TrackballCameraController>(
					m_GLFWHandle.window());
			}
			else
			{
				cameraController = std::make_unique<FirstPersonCameraController>(
					m_GLFWHandle.window());
			}

			cameraController->setCamera(oldCamera);
		}

		if (ImGui::CollapsingHeader("Light"), &lightHeader)
		{
			if (ImGui::SliderFloat("Vertical", &lightAngleV, 0.0f, 3.14f)
			|| ImGui::SliderFloat("Horizontal", &lightAngleH, 0.0f, 6.28f))
			{
				lightDirectionRaw = glm::vec3(
					sinf(lightAngleV) * cosf(lightAngleH),
					cosf(lightAngleV),
					sinf(lightAngleV) * sinf(lightAngleH));
			}

			ImGui::ColorEdit3("Color#lightColor", (float*) &lightRadiance, 0);

			ImGui::Checkbox("Bind to Camera", &lightFromCamera);
		}
      }

      ImGui::End();
    }

    imguiRenderFrame();

    glfwPollEvents(); // Poll for and process events

    auto ellapsedTime = glfwGetTime() - seconds;
    auto guiHasFocus =
        ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    if (!guiHasFocus) {
      cameraController->update(float(ellapsedTime));
    }

    m_GLFWHandle.swapBuffers(); // Swap front and back buffers
  }

  // TODO clean up allocated GL data

  return 0;
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
    uint32_t height, const fs::path &gltfFile, const fs::path &cubeMapFile,
    const std::vector<float> &lookatArgs, const std::string &vertexShader,
    const std::string &fragmentShader, const fs::path &output) :
    m_nWindowWidth(width),
    m_nWindowHeight(height),
    m_AppPath{appPath},
    m_AppName{m_AppPath.stem().string()},
    m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
    m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
    m_gltfFilePath{gltfFile},
    m_cubeMapFilePath{cubeMapFile},
    m_OutputPath{output}
{
  if (!lookatArgs.empty()) {
    m_hasUserCamera = true;
    m_userCamera =
        Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
            glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
            glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
  }

  if (!vertexShader.empty()) {
    m_vertexShader = vertexShader;
  }

  if (!fragmentShader.empty()) {
    m_fragmentShader = fragmentShader;
  }

  ImGui::GetIO().IniFilename =
      m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
                                  // positions in this file

  glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

  printGLVersion();
}
