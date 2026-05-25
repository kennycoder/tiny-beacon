# Build stage
FROM golang:1.25-alpine AS builder

WORKDIR /app

# Install git and ca-certificates
RUN apk add --no-cache git ca-certificates

# Copy go.mod and go.sum first to leverage Docker caching
COPY go.mod go.sum ./
RUN go mod download

# Copy the rest of the source code
COPY . .

# Build a statically linked Go binary
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-w -s" -o server .

# Runtime stage
FROM alpine:latest

WORKDIR /app

# Install ca-certificates and tzdata
RUN apk --no-cache add ca-certificates tzdata

# Copy binary and static assets from builder stage
COPY --from=builder /app/server .
COPY --from=builder /app/static ./static

# Cloud Run automatically sets the PORT environment variable
EXPOSE 8080

# Start the server
CMD ["./server"]
