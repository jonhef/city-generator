import Vapor

final class GenerationController {
    private let generator: GeneratorService

    init(generator: GeneratorService) {
        self.generator = generator
    }

    func generate(req: Request) async throws -> GenerationResponse {
        let payload = try req.content.decode(GenerationRequest.self)
        let city = try await generator.generate(request: payload, on: req.eventLoop).get()
        return GenerationResponse(from: city)
    }

    func listCities(req: Request) async throws -> [GenerationResponse] {
        let cities = try await generator.listCities(on: req.eventLoop).get()
        return cities.map { GenerationResponse(from: $0) }
    }

    func getSummary(req: Request) async throws -> Response {
        guard let id = req.parameters.get("id") else {
            throw Abort(.badRequest, reason: "Missing city identifier")
        }
        let path = try await generator.summaryPath(for: id, on: req.eventLoop).get()
        return try await req.fileio.asyncStreamFile(at: path.path)
    }

    func getModel(req: Request) async throws -> Response {
        guard let id = req.parameters.get("id") else {
            throw Abort(.badRequest, reason: "Missing city identifier")
        }
        let path = try await generator.modelPath(for: id, on: req.eventLoop).get()
        return try await req.fileio.asyncStreamFile(at: path.path)
    }
}
